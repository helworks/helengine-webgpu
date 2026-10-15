"""Fetch only pinned compiler dependencies using Dawn's own DEPS evaluator."""
import importlib.util
import pathlib
import sys
from types import SimpleNamespace

root = pathlib.Path(sys.argv[1]).resolve()
module_spec = importlib.util.spec_from_file_location("dawn_dependencies", root / "tools" / "fetch_dawn_dependencies.py")
module = importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(module)
module.process_dir(SimpleNamespace(git="git", shallow=True), root, [
    "third_party/abseil-cpp", "third_party/spirv-headers/src", "third_party/spirv-tools/src",
    "third_party/jinja2", "third_party/markupsafe", "third_party/webgpu-headers/src",
])
