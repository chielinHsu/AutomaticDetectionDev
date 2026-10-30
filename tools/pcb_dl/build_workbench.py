"""Offline qmake payload build: validate runtime, compile, and stage application sources."""
import argparse
import compileall
import importlib.metadata
import json
from pathlib import Path
import shutil
import struct
import sys


def build(source,output):
    if sys.version_info[:2]!=(3,12) or struct.calcsize('P')!=8:raise ValueError('Python 3.12 x64 is required')
    versions={name:importlib.metadata.version(name) for name in ('PySide6','numpy','opencv-python','onnxruntime','torch','ultralytics')}
    source=Path(source).resolve();output=Path(output).resolve()
    package=source/'tools/pcb_dl'
    if not (package/'cli.py').is_file():raise ValueError('Workbench source tree was not found')
    if output==source or source.is_relative_to(output) or output.is_relative_to(package):raise ValueError('Output must be outside the source package')
    files=[p for p in package.iterdir() if p.suffix in ('.py','.txt')]
    # Syntax validation happens before changing the previous runnable payload.
    for path in files:
        if path.suffix=='.py':compile(path.read_text(encoding='utf-8-sig'),str(path),'exec')
    destination=output/'tools/pcb_dl';destination.mkdir(parents=True,exist_ok=True)
    old_manifest=output/'runtime.json'
    if old_manifest.exists():old_manifest.unlink()
    (output/'tools/__init__.py').write_text('',encoding='utf-8')
    for path in files:shutil.copy2(path,destination/path.name)
    # Remove only stale direct package files previously managed by this builder.
    for path in destination.iterdir():
        if path.is_file() and path.suffix in ('.py','.txt') and path.name not in {p.name for p in files}:path.unlink()
    if not compileall.compile_dir(str(output/'tools'),quiet=1,force=True):raise ValueError('Python bytecode compilation failed')
    runtime=dict(format='vision-cad-runtime',version=1,python=str(Path(sys.executable).resolve()),versions=versions)
    old_manifest.write_text(json.dumps(runtime,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'Vision CAD workbench built: {output} ({len(files)} files; Python {sys.version.split()[0]})')


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--source',required=True);parser.add_argument('--output',required=True);args=parser.parse_args()
    try:build(args.source,args.output)
    except Exception as error:print(f'Workbench build failed: {error}',file=sys.stderr);return 1
    return 0


if __name__=='__main__':sys.exit(main())
