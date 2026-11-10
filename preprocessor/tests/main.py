import sys
from pathlib import Path
parent_dir = str(Path(__file__).resolve().parent.parent)
sys.path.append(parent_dir)

from json_parser import parse_json
from preprocessor import form_total_results

DATASET_FOLDER = "./spec/datasets/"
EXPTECED_RESULTS_FILE = "./preprocessor/tests/expected_results.json"

FILES = ("01_small.json", "02_boundaries.json", "03_gap_20min.json", "04_invalid_values.json", "05_long_record.json", "06_empty.json", "07_single_point.json")
ATTRIBUTES = ("status", "sample_count", "unknown_s", "working_s", "span_s", "stopped_s", "invalid_s", "gap_s")

RED   = "\033[31m"
GREEN = "\033[32m"
RESET = "\033[0m"

# TODO: Compare each result with the corresponding entry in "expected_results.json"
def main():
    expected_results_json = str()
    with open(EXPTECED_RESULTS_FILE) as f:
        expected_results_json = f.read()
    expected_results = parse_json(expected_results_json)

    for file in FILES:
        expected_result = getattr(expected_results, file)
        result_json = form_total_results(DATASET_FOLDER + file)
        result = getattr(parse_json(result_json), file)
        if compare_samples(expected_result, result) == True:
            print(f"{file}: {GREEN}Ok{RESET}")
        else:
            print(f"{file}: {RED}Don't match{RESET}")

def compare_samples(sample1, sample2):
    for attr in ATTRIBUTES:
        sample1_attr = getattr(sample1, attr)
        sample2_attr = getattr(sample2, attr)
        if (sample1_attr != sample2_attr):
            return False
    return True

if __name__ == "__main__":
    main()
