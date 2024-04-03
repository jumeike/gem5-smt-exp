#include <math.h>
#include <stdlib.h>
#include <time.h>

typedef int result_type;

typedef struct {
    result_type _M_a;
    result_type _M_b;
    double _M_theta;
    double _M_zeta;
    double _M_zeta2theta;
} zipfian_int_distribution_param_type;

typedef struct {
    zipfian_int_distribution_param_type _M_param;
} zipfian_int_distribution;

double custom_pow(double base, int exponent) {
    double result = 1.0;
    for (int i = 0; i < exponent; ++i) {
        result *= base;
    }
    return result;
}

double zeta(unsigned long __n, double __theta) {
    double ans = 0.0;
    for (unsigned long i = 1; i <= __n; ++i)
        ans += custom_pow(1.0 / i, __theta);
    return ans;
}

void zipfian_int_distribution_param_type_init(zipfian_int_distribution_param_type *param,
    result_type __a, result_type __b, double __theta) {
    param->_M_a = __a;
    param->_M_b = __b;
    param->_M_theta = __theta;
    param->_M_zeta = zeta(__b - __a + 1, __theta);
    param->_M_zeta2theta = zeta(2, __theta);
}

void zipfian_int_distribution_init(zipfian_int_distribution *dist, result_type __a, result_type __b, double __theta) {
    zipfian_int_distribution_param_type param;
    zipfian_int_distribution_param_type_init(&param, __a, __b, __theta);
    dist->_M_param = param;
}

result_type zipfian_int_distribution_operator_param(zipfian_int_distribution *dist, void *urng, zipfian_int_distribution_param_type param);

result_type zipfian_int_distribution_operator(zipfian_int_distribution *dist, void *urng) {
    return zipfian_int_distribution_operator_param(dist, urng, dist->_M_param);
}

result_type zipfian_int_distribution_operator_param(zipfian_int_distribution *dist, void *urng, zipfian_int_distribution_param_type param) {
    double alpha = 1 / (1 - param._M_theta);
    double eta = (1 - custom_pow(2.0 / (param._M_b - param._M_a + 1), 1 - param._M_theta)) / (1 - param._M_zeta2theta / param._M_zeta);

    double u = (double)rand() / (double)RAND_MAX;

    double uz = u * param._M_zeta;
    if (uz < 1.0) return param._M_a;
    if (uz < 1.0 + custom_pow(0.5, param._M_theta)) return param._M_a + 1;

    return param._M_a + ((param._M_b - param._M_a + 1) * custom_pow(eta * u - eta + 1, alpha));
}


