import subprocess
import sys

def run_test_case(input_file, output_file, program_file):
    with open(input_file, 'r') as f:
        input_data = f.read()

    result = subprocess.run(
        [sys.executable, program_file],
        input=input_data,
        text=True,
        capture_output=True,
        timeout=12
    )
    actual_output = result.stdout.strip()
    with open(output_file, 'r') as f:
        expected_output = f.read().strip()
    return actual_output == expected_output

def run_all_test_cases(program_file):
    for i in range(11):
        input_file = f'testcases/input{i}.txt'
        output_file = f'testcases/output{i}.txt'
        if run_test_case(input_file, output_file, program_file):
            print(f'Test case {i}: PASSED')
        else:
            print(f'Test case {i}: FAILED')

if __name__ == "__main__":
    program_file = 'Greedy_or_not.py'
    run_all_test_cases(program_file)