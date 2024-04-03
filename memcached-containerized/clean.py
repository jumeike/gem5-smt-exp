def clean_dataset_and_save(input_file, output_file):
    cleaned_lines = []

    with open(input_file, 'r') as file:
        for line in file:
            # Remove leading and trailing whitespace (including newlines)
            cleaned_line = line.strip()
            cleaned_lines.append(cleaned_line)

    with open(output_file, 'w') as file:
        for line in cleaned_lines:
            file.write(line + '\n')

# Provide the paths to your input and output files
input_file = "vals"
output_file = "vals_n"

# Clean the dataset and save the corrected dataset to a new file
clean_dataset_and_save(input_file, output_file)