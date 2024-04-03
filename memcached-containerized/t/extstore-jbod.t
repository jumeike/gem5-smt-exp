#!/usr/bin/env perl

use strict;
use warnings;
use Test::More;
use FindBin qw($Bin);
use lib "$Bin/lib";
use MemcachedTest;
use Data::Dumper qw/Dumper/;

my $ext_path;
my $ext_path2;

if (!supports_extstore()) {
    plan skip_all => 'extstore not enabled';
    exit 0;
}

$ext_path = "/tmp/extstore1.$$";
$ext_path2 = "/tmp/extstore2.$$";

my $server = new_memcached("-m 256 -U 0 -o ext_page_size=8,ext_wbuf_size=2,ext_threads=1,ext_io_depth=2,ext_item_size=512,ext_item_age=2,ext_recache_rate=10000,ext_max_frag=0.9,ext_path=$ext_path:64m,ext_path=$ext_path2:96m,slab_automove=1,ext_max_sleep=100000");
my $sock = $server->sock;

my $value;
{
    my @chars = ("C".."Z");
    for (1 .. 20000) {
        $value .= $chars[rand @chars];
    }
}

# fill some larger objects
{
    my $free_before = summarize_buckets(mem_stats($sock, ' extstore'));
    # we gave 64m and 96m devices, so pages in DEFAULT should be > 8 or 12 to
    # be sure they're both in the same pool.
    cmp_ok($free_before->[0], '>', 18, "default has more pages");
    my $keycount = 4000;
    for (1 .. $keycount) {
        print $sock "set nfoo$_ 0 0 20000 noreply\r\n$value\r\n";
        print $sock "set lfoo$_ 0 0 20000 noreply\r\n$value\r\n";
    }
    # wait for a flush
    wait_ext_flush($sock);
    # TODO: this is failing only on github actions and I have no idea why.
    #mem_get_is($sock, "nfoo1", $value);
    # fill to excess
    for (1 .. $keycount) {
        print $sock "set kfoo$_ 0 0 20000 noreply\r\n$value\r\n";
        print $sock "set zfoo$_ 0 0 20000 noreply\r\n$value\r\n";
    }
    wait_ext_flush($sock);

    my $free_after = summarize_buckets(mem_stats($sock, ' extstore'));
    # delete half
    for (1 .. $keycount) {
        print $sock "delete lfoo$_ noreply\r\n";
    }
    print $sock "lru_crawler crawl all\r\n";
    <$sock>;

    cmp_ok($free_after->[0], '<', 4, "default is mostly full");
    # fetch
    # check extstore counters
    my $stats = mem_stats($sock);
    is($stats->{evictions}, 0, 'no RAM evictions');
    cmp_ok($stats->{extstore_page_allocs}, '>', 0, 'at least one page allocated');
    cmp_ok($stats->{extstore_objects_written}, '>', $keycount / 2, 'some objects written');
    cmp_ok($stats->{extstore_bytes_written}, '>', length($value) * 2, 'some bytes written');
    # commented out because we're not testing fetching data back in this test.
    #cmp_ok($stats->{get_extstore}, '>', 0, 'one object was fetched');
    #cmp_ok($stats->{extstore_objects_read}, '>', 0, 'one object read');
    #cmp_ok($stats->{extstore_bytes_read}, '>', length($value), 'some bytes read');
    cmp_ok($stats->{extstore_page_evictions}, '>', 0, 'at least one page evicted');
    cmp_ok($stats->{extstore_page_reclaims}, '>', 1, 'at least two pages reclaimed');
}

sub summarize_buckets {
    my $s = shift;
    my @buks = ();
    for (0 .. 6) {
        push(@buks, 0);
    }
    my $is_free = 0;
    my $x = 0;
    while (exists $s->{$x . ':version'}) {
        #my $fb = $s->{$x . ':free_bucket'};
        #print STDERR "BYTES: [$x:$fb] ", $s->{$x . ':bytes'}, "\n";
        my $is_used = $s->{$x . ':version'};
        if ($is_used == 0) {
            # version of 0 means the page is free.
            $buks[$s->{$x . ':free_bucket'}]++;
        }
        $x++;
    }
    return \@buks;
}

done_testing();

END {
    unlink $ext_path if $ext_path;
    unlink $ext_path2 if $ext_path2;
}
