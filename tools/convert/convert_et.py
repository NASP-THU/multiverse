#!/usr/bin/env python3
"""
Convert Chakra ET files to JSON or PDF format.
Supports single files, directories, and archives (tar.gz, zip, etc.)
"""
import sys
import os
import json
import re
import tempfile
import tarfile
import zipfile
import shutil
from pathlib import Path
from datetime import datetime
from typing import List, Tuple, Optional

os.environ['PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION'] = 'python'

from google.protobuf.json_format import MessageToDict
from chakra.schema.protobuf.et_def_pb2 import GlobalMetadata, Node
from chakra.src.third_party.utils.protolib import decodeMessage as decode_message
from chakra.src.third_party.utils.protolib import openFileRd as open_file_rd

# PDF format support (optional, will be checked at runtime)
try:
    import graphviz
    HAS_GRAPHVIZ = True
except ImportError:
    HAS_GRAPHVIZ = False


class InputHandler:
    SUPPORTED_ARCHIVES = {'.tar.gz', '.tar.bz2', '.tar.xz', '.tgz', '.zip'}
    
    def __init__(self, input_path: str):
        self.input_path = Path(input_path).resolve()
        self.input_type = self._detect_input_type()
        
    def _detect_input_type(self) -> str:
        if not self.input_path.exists():
            raise FileNotFoundError(f"Input path does not exist: {self.input_path}")
            
        if self.input_path.is_file():
            if self._is_archive_file():
                return 'archive'
            elif self.input_path.suffix == '.et':
                return 'file'
            else:
                raise ValueError(f"Unsupported file type: {self.input_path.suffix}. Expected .et file or supported archive.")
        elif self.input_path.is_dir():
            return 'directory'
        else:
            raise ValueError(f"Invalid input path: {self.input_path}")
    
    def _is_archive_file(self) -> bool:
        path_str = str(self.input_path).lower()
        return any(path_str.endswith(ext) for ext in self.SUPPORTED_ARCHIVES)
    
    def get_et_files(self) -> List[Path]:
        if self.input_type == 'file':
            return [self.input_path]
        elif self.input_type == 'directory':
            return list(self.input_path.glob('**/*.et'))
        else:  # archive
            raise ValueError("Archive files must be extracted first. Use ArchiveExtractor.")
    
    def validate_input(self) -> bool:
        if self.input_type == 'archive':
            return True
        
        et_files = self.get_et_files()
        if not et_files:
            raise ValueError(f"No .et files found in {self.input_path}")
        return True


class ArchiveExtractor:
    def __init__(self, archive_path: Path, temp_dir: Optional[Path] = None):
        self.archive_path = archive_path
        self.temp_dir = temp_dir or Path(tempfile.mkdtemp(prefix='chakra_extract_'))
        self.extracted_path = None
        
    def extract(self) -> Path:
        print(f"📦 Extracting archive: {self.archive_path.name}")
        
        try:
            if str(self.archive_path).lower().endswith('.zip'):
                self._extract_zip()
            elif any(str(self.archive_path).lower().endswith(ext) for ext in ['.tar.gz', '.tgz', '.tar.bz2', '.tar.xz']):
                self._extract_tar()
            else:
                raise ValueError(f"Unsupported archive format: {self.archive_path}")
                
            # Find the actual extraction directory (handle nested structures)
            extracted_dirs = [d for d in self.temp_dir.iterdir() if d.is_dir()]
            if len(extracted_dirs) == 1:
                self.extracted_path = extracted_dirs[0]
            else:
                self.extracted_path = self.temp_dir
                
            et_files = list(self.extracted_path.glob('**/*.et'))
            print(f"📁 Extracted {len(et_files)} ET files to: {self.extracted_path}")
            
            if not et_files:
                raise ValueError(f"No .et files found in extracted archive: {self.archive_path}")
                
            return self.extracted_path
            
        except Exception as e:
            self.cleanup()
            raise e
    
    def _extract_zip(self):
        with zipfile.ZipFile(self.archive_path, 'r') as zip_ref:
            zip_ref.extractall(self.temp_dir)
    
    def _extract_tar(self):
        with tarfile.open(self.archive_path, 'r:*') as tar_ref:
            tar_ref.extractall(self.temp_dir)
    
    def cleanup(self):
        if self.temp_dir.exists():
            shutil.rmtree(self.temp_dir)
            print(f"🧹 Cleaned up temporary directory: {self.temp_dir}")


class OutputManager:
    """Manages output directory and file paths for ET conversion."""
    
    def __init__(self, output_dir: Optional[str], input_handler: InputHandler, output_format: str = 'json'):
        self.output_format = output_format
        self.input_handler = input_handler
        self.output_ext = f'.{output_format}'
        
        # Determine output directory
        if output_dir:
            self.output_dir = Path(output_dir).resolve()
        elif input_handler.input_type == 'directory':
            # For directory input, default to ./output
            self.output_dir = Path.cwd() / 'output'
        else:
            # For single file input, default to current directory
            self.output_dir = Path.cwd()
        
        # Ensure output directory exists
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
    def generate_output_paths(self, et_files: List[Path]) -> List[Tuple[Path, Path]]:
        """Generate input-output file pairs."""
        pairs = []
        for et_file in et_files:
            output_filename = f"{et_file.stem}{self.output_ext}"
            output_file = self.output_dir / output_filename
            pairs.append((et_file, output_file))
        return pairs


class BatchProcessor:
    def __init__(self, output_format: str = 'json'):
        self.output_format = output_format
        self.total_files = 0
        self.processed_files = 0
        self.total_objects = 0
        self.failed_files = []
        
    def process_files(self, file_pairs: List[Tuple[Path, Path]]) -> dict:
        self.total_files = len(file_pairs)
        self.processed_files = 0
        self.total_objects = 0
        self.failed_files = []
        
        print(f"🚀 Starting batch conversion of {self.total_files} ET files...")
        
        for i, (input_file, output_file) in enumerate(file_pairs, 1):
            try:
                print(f"\n[{i}/{self.total_files}] Converting: {input_file.name}")
                
                # Choose conversion function based on output format
                if self.output_format == 'pdf':
                    object_count = convert_et_to_pdf(str(input_file), str(output_file))
                else:
                    object_count = convert_et_to_json(str(input_file), str(output_file))
                    
                self.total_objects += object_count
                self.processed_files += 1
                
                # Show progress
                progress = (i / self.total_files) * 100
                print(f"📊 Progress: {progress:.1f}% ({i}/{self.total_files} files)")
                
            except Exception as e:
                error_msg = f"Failed to convert {input_file.name}: {str(e)}"
                print(f"❌ {error_msg}")
                self.failed_files.append((input_file, str(e)))
                
        return self._generate_summary()
    
    def _generate_summary(self) -> dict:
        return {
            'total_files': self.total_files,
            'processed_files': self.processed_files,
            'failed_files': len(self.failed_files),
            'total_objects': self.total_objects,
            'failures': self.failed_files
        }


def convert_et_to_json(input_file: str, output_file: str) -> int:
    input_path = Path(input_file)
    output_path = Path(output_file)
    
    print(f"📄 Converting {input_path.name} → {output_path.name}")
    
    execution_trace = open_file_rd(input_file)
    node = Node()
    json_objects = []
    
    # Read global metadata first
    global_metadata = GlobalMetadata()
    decode_message(execution_trace, global_metadata)
    metadata_dict = MessageToDict(global_metadata)
    
    if not (len(metadata_dict) == 1 and 'version' in metadata_dict):
        json_objects.append(metadata_dict)
    
    node_count = 0
    while decode_message(execution_trace, node):
        node_dict = MessageToDict(node)
        json_objects.append(node_dict)
        node_count += 1
        
        if node_count % 5000 == 0:
            print(f"Processed {node_count} nodes...")
            
    execution_trace.close()
    
    print(f"Writing {len(json_objects)} objects to {output_file}...")
    with open(output_file, "w") as file:
        json.dump(json_objects, file, indent=2)
    
    print(f"✅ Successfully converted {node_count} nodes to standard JSON array")
    
    file_size = os.path.getsize(output_file) / (1024 * 1024)
    print(f"📁 Output file size: {file_size:.1f} MB")
    
    return len(json_objects)


def escape_label(label: str) -> str:
    """Escapes special characters in labels for graph rendering."""
    special_chars = "{}()<>\\[\\]|&-"
    return re.sub(f"([{special_chars}])", r"\\\1", label)


def convert_et_to_pdf(input_file: str, output_file: str) -> int:
    """Convert ET file to PDF visualization format using graphviz."""
    if not HAS_GRAPHVIZ:
        raise ImportError("graphviz package is required for PDF conversion. Install with: pip install graphviz")
    
    input_path = Path(input_file)
    output_path = Path(output_file)
    
    print(f"📄 Converting {input_path.name} → {output_path.name} (PDF)")
    
    execution_trace = open_file_rd(input_file)
    node = Node()
    
    # Create graphviz digraph
    f = graphviz.Digraph()
    
    # Color mapping for node types
    node_colors = {
        'COMP_NODE': '#90EE90',           # Light green
        'COMM_COLL_NODE': '#87CEEB',      # Sky blue
        'COMM_SEND_NODE': '#FFB6C1',      # Light pink
        'COMM_RECV_NODE': '#DDA0DD',      # Plum
        'MEM_LOAD_NODE': '#F0E68C',       # Khaki
        'MEM_STORE_NODE': '#FFA07A',      # Light salmon
    }
    default_color = '#D3D3D3'  # Light gray
    
    # Read global metadata first
    global_metadata = GlobalMetadata()
    decode_message(execution_trace, global_metadata)
    
    node_count = 0
    while decode_message(execution_trace, node):
        # Format: id:name
        display_label = f"{node.id}:{node.name}"
        escaped_label = escape_label(display_label)
        
        # Get node type and corresponding color
        node_dict = MessageToDict(node)
        node_type = node_dict.get('type', '')
        fill_color = node_colors.get(node_type, default_color)
        
        f.node(name=f"{node.id}", label=escaped_label, id=str(node.id), 
               shape="record", style="filled", fillcolor=fill_color)

        # Handling data dependencies
        for data_dep_id in node.data_deps:
            f.edge(str(data_dep_id), str(node.id), arrowhead="normal")

        # Handling control dependencies
        for ctrl_dep_id in node.ctrl_deps:
            f.edge(str(ctrl_dep_id), str(node.id), arrowhead="tee")

        node_count += 1
        if node_count % 1000 == 0:
            print(f"Processed {node_count} nodes...")

    execution_trace.close()
    
    # Render PDF
    output_base = str(output_path).replace(".pdf", "")
    f.render(output_base, format="pdf", cleanup=True)
    
    print(f"✅ Successfully converted {node_count} nodes to PDF visualization")
    
    if output_path.exists():
        file_size = os.path.getsize(output_file) / (1024 * 1024)
        print(f"📁 Output file size: {file_size:.1f} MB")
    
    return node_count


def process_input(input_path: str, output_dir: Optional[str] = None, output_format: str = 'json') -> dict:
    input_handler = InputHandler(input_path)
    input_handler.validate_input()
    
    extractor = None
    temp_cleanup_needed = False
    
    try:
        if input_handler.input_type == 'archive':
            extractor = ArchiveExtractor(input_handler.input_path)
            extracted_path = extractor.extract()
            input_handler = InputHandler(str(extracted_path))
            temp_cleanup_needed = True
        
        et_files = input_handler.get_et_files()
        print(f"📂 Found {len(et_files)} ET file(s) to process")
        
        output_manager = OutputManager(output_dir, input_handler, output_format)
        file_pairs = output_manager.generate_output_paths(et_files)
        
        print(f"📁 Output directory: {output_manager.output_dir}")
        print(f"📋 Output format: {output_format.upper()}")
        
        if len(file_pairs) == 1:
            input_file, output_file = file_pairs[0]
            print(f"\n🚀 Converting single file...")
            
            # Choose conversion function based on output format
            if output_format == 'pdf':
                object_count = convert_et_to_pdf(str(input_file), str(output_file))
            else:
                object_count = convert_et_to_json(str(input_file), str(output_file))
            
            summary = {
                'total_files': 1,
                'processed_files': 1,
                'failed_files': 0,
                'total_objects': object_count,
                'failures': [],
                'output_files': [str(output_file)]
            }
        else:
            processor = BatchProcessor(output_format)
            summary = processor.process_files(file_pairs)
            summary['output_files'] = [str(pair[1]) for pair in file_pairs]
        
        summary['output_format'] = output_format
        return summary
        
    finally:
        if temp_cleanup_needed and extractor:
            extractor.cleanup()


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Chakra ET Converter - Convert ET files/directories/archives to JSON or PDF',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Usage Examples:
  Single file conversion (JSON, default):
    %(prog)s --input trace.et
    %(prog)s --input trace.et --output ./results/
    
  Single file conversion (PDF visualization):
    %(prog)s --input trace.et --format pdf
    %(prog)s --input trace.et --format pdf --output ./results/
    
  Directory batch conversion:
    %(prog)s --input ./traces/
    %(prog)s --input ./traces/ --output ./json_outputs/
    %(prog)s --input ./traces/ --format pdf --output ./pdf_outputs/
    
  Archive extraction and conversion:
    %(prog)s --input traces.tar.gz --output ./extracted/
    %(prog)s --input data.zip --format pdf

Supported Output Formats:
  json  - Standard JSON array format (default)
  pdf   - PDF visualization with graphviz (requires graphviz package)
    
"""
    )
    
    parser.add_argument('--input', '-i', type=str, required=True,
                       help='Input path (ET file, directory, or archive). Supports: .et files, directories with .et files, .tar.gz, .tar.bz2, .tar.xz, .zip archives')
    parser.add_argument('--output', '-o', type=str, default=None,
                       help='Output directory. Default: current directory for single file, ./output for directory input')
    parser.add_argument('--format', '-f', type=str, choices=['json', 'pdf'], default='json',
                       help='Output format: json (default) or pdf')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Enable verbose output')
    
    args = parser.parse_args()
    
    
    try:
        print("🚀 Chakra ET Converter Starting...")
        print(f"📂 Input:  {args.input}")
        
        # Process input
        summary = process_input(args.input, args.output, args.format)
        
        # Display results
        print(f"\n🎉 Conversion Complete!")
        print(f"\n📊 Summary:")
        print(f"  📂 Total files processed: {summary['processed_files']}/{summary['total_files']}")
        print(f"  📦 Total objects converted: {summary['total_objects']:,}")
        
        if summary['failed_files'] > 0:
            print(f"  ❌ Failed conversions: {summary['failed_files']}")
            if args.verbose:
                for failed_file, error in summary['failures']:
                    print(f"    - {failed_file}: {error}")
        
        if summary['processed_files'] > 0:
            print(f"  📁 Output files:")
            for output_file in summary['output_files'][:5]:  # Show first 5 files
                print(f"    - {output_file}")
            if len(summary['output_files']) > 5:
                print(f"    ... and {len(summary['output_files']) - 5} more files")
            
            # Show format based on actual format used
            if summary.get('output_format') == 'pdf':
                print(f"  📋 Format: PDF visualization (graphviz)")
            else:
                print(f"  📋 Format: Pretty-printed JSON arrays")
        
        # Exit with appropriate code
        exit_code = 0 if summary['failed_files'] == 0 else 1
        if exit_code != 0:
            print(f"\n⚠️  Warning: {summary['failed_files']} file(s) failed to convert")
        
        sys.exit(exit_code)
        
    except Exception as e:
        print(f"\n❌ Error: {str(e)}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
