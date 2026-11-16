# Chakra ET Converter

Convert ET files to JSON or PDF format. Supports single files, directories, and archives.

## Requirements

- Python >= 3.10.2
- graphviz (optional, for PDF output)

## Quick Start

```bash
# 1. Create virtual environment (or other virtual environments like Anaconda)
python3.10 -m venv multiverse_env
source multiverse_env/bin/activate  # Linux/macOS (Windows: multiverse_env\Scripts\activate)

# 2. Install chakra and update protobuf version to 6.31.1
pip install https://github.com/mlcommons/chakra/archive/refs/heads/main.zip
pip install "protobuf==6.31.1"

# 3. (Optional) Install graphviz for PDF visualization support
pip install graphviz
# Also need system graphviz: apt install graphviz (Ubuntu) or brew install graphviz (macOS)

# 4. Run conversion
python convert_et.py --input your_file.et
```

## Supported Output Formats

- `json` - Standard JSON array format (default)
- `pdf` - PDF visualization with graphviz (requires graphviz package)

## Usage

```bash
# Single file (JSON, output to current directory)
python convert_et.py --input trace.et
python convert_et.py --input trace.et --output ./results/

# Single file (PDF visualization)
python convert_et.py --input trace.et --format pdf
python convert_et.py --input trace.et --format pdf --output ./results/

# Directory batch conversion (output to ./output by default)
python convert_et.py --input ./et_files/
python convert_et.py --input ./et_files/ --output ./json_outputs/
python convert_et.py --input ./et_files/ --format pdf --output ./pdf_outputs/

# Archive extraction and conversion (output to ./output by default)
python convert_et.py --input data.tar.gz
python convert_et.py --input data.tar.gz --output ./json_outputs/
python convert_et.py --input data.tar.gz --format pdf --output ./pdf_outputs/
```

## Parameters

| Parameter | Short | Required | Default | Description |
|-----------|-------|----------|---------|-------------|
| `--input` | `-i` | Yes | - | Input ET file, directory, or archive |
| `--output` | `-o` | No | `.` or `./output` | Output directory |
| `--format` | `-f` | No | `json` | Output format: `json` or `pdf` |
| `--verbose` | `-v` | No | - | Enable verbose output |

