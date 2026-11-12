"""Verification runner for the plugins in this repository.

Every plugin is verified on its own: it is staged from an allowlist, built with BuildPlugin,
installed into a freshly generated host project that contains only that plugin and the test
module of each of its modules, and tested with the editor's automation framework. The final guards
run after the packages exist, so the denylist scan covers staged and packaged files. Guards check
the repository rules that a compiler cannot see. Run with the Python interpreter bundled with the
engine, for example:

    <UE>/Engine/Binaries/ThirdParty/Python3/Win64/python.exe Tests/run.py all --engine <UE>

See Tests/docs/running.md for every command and what it proves.
"""

import argparse
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
TESTS = REPO / "Tests"
TMP = REPO / "tmp"
PLUGINS = ["UnrealUtilities", "TextProcessing", "StreamingAudio", "AnimationDataTools"]
# Host module every generated host carries: test helpers that are not tied to one plugin.
SUPPORT_MODULE = "HostTestSupport"

# Paths of a plugin directory that may be packaged. Anything else (scratch output, plans,
# build products) never reaches a stage, because BuildPlugin copies a plugin directory whole.
STAGE_ALLOWLIST = ["{plugin}.uplugin", "LICENSE", "README.md", "Config", "Content", "Resources", "Source", "docs"]

# Package entries BuildPlugin produces in addition to the staged ones.
PACKAGE_EXTRA = {"Binaries", "Intermediate"}

RETIRED_IDENTIFIERS = [
    "CUSTOMLIBRARY_API", "FCustomLibraryModule", "UCustomBPLibrary", "CustomBPLibrary",
    "CustomCppLibrary", "CustomBFLibrary", "LogCustomBPLibrary",
    "BlueprintGapHelpers", "UBlueprintGapHelpersLibrary", "ChannelAnimation", "IncrementalText",
]
RETIRED_FILE_NAMES = ["CustomLibrary.uplugin", "BlueprintGapHelpers.uplugin", "ChannelAnimation.uplugin", "IncrementalText.uplugin"]
RETIRED_DECLARATIONS = [
    "RandomWeightIndex", "IsActorInFrustrum", "GetLockedAxis", "CustomLog", "RayPlaneIntersection",
    "GetProjectVersion", "GetCmdParameter", "RegexReplace", "GetHMDViewOffset",
    "GetHMDProjectionMatix", "CustomShuffleArray", "GetPointersFromWeak",
]
REDIRECT_KEYS = re.compile(
    r"^\s*[+\-.!]?\s*(ActiveClassRedirects|ActiveStructRedirects|ActiveGameNameRedirects|"
    r"ClassRedirects|StructRedirects|EnumRedirects|FunctionRedirects|PropertyRedirects|"
    r"PackageRedirects)\s*=|^\s*\[CoreRedirects\]",
    re.IGNORECASE | re.MULTILINE,
)
# Documents every architecture owner carries, relative to the owner directory.
OWNER_DOCS = [
    "README.md", "docs/README.md", "docs/architecture/README.md", "docs/architecture/structure.md",
    "docs/architecture/diagrams/README.md", "docs/architecture/diagrams/main.md",
]
MARKDOWN_LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")
TEXT_SUFFIXES = {".h", ".cpp", ".cs", ".uplugin", ".uproject", ".ini", ".md", ".py", ".json", ".txt", ".in"}
SOURCE_SUFFIXES = {".h", ".cpp"}


class RunnerError(Exception):
    pass


def log(message):
    print(message, flush=True)


def remove_tree(path):
    """Removes a tree, clearing the read-only bit on Windows crash and build files first."""
    def on_error(function, failed_path, exc_info):
        os.chmod(failed_path, stat.S_IWRITE)
        function(failed_path)
    if path.exists():
        shutil.rmtree(path, onerror=on_error)


# ---------------------------------------------------------------------------------------------
# Engine
# ---------------------------------------------------------------------------------------------

def resolve_engine(engine_arg):
    candidate = engine_arg or os.environ.get("UE_ENGINE_ROOT")
    if not candidate:
        raise RunnerError("Pass --engine <engine root> or set UE_ENGINE_ROOT.")
    root = Path(candidate)
    version_file = root / "Engine" / "Build" / "Build.version"
    if not version_file.is_file():
        raise RunnerError(f"Not an engine root (missing {version_file}).")
    return root


def engine_version(root):
    data = json.loads((root / "Engine" / "Build" / "Build.version").read_text(encoding="utf-8"))
    return f'{data["MajorVersion"]}.{data["MinorVersion"]}.{data["PatchVersion"]}'


def run_logged(command, log_path, timeout=None, cwd=None):
    """Runs a command, writing its combined output to log_path. Returns (exit_code, timed_out)."""
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with open(log_path, "w", encoding="utf-8", errors="replace") as handle:
        handle.write(" ".join(str(part) for part in command) + "\n\n")
        handle.flush()
        process = subprocess.Popen(command, stdout=handle, stderr=subprocess.STDOUT, cwd=cwd)
        try:
            return process.wait(timeout=timeout), False
        except subprocess.TimeoutExpired:
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            process.wait()
            return None, True


# ---------------------------------------------------------------------------------------------
# Staging and packaging
# ---------------------------------------------------------------------------------------------

def allowlist_for(plugin):
    return [entry.format(plugin=plugin) for entry in STAGE_ALLOWLIST]


def plugin_modules(plugin, plugins_root=None):
    """Module names of a plugin: every Source/ subdirectory that holds its own Build.cs."""
    source = (plugins_root or REPO / "Plugins") / plugin / "Source"
    if not source.is_dir():
        return []
    return sorted(m.name for m in source.iterdir() if (m / f"{m.name}.Build.cs").is_file())


def test_module_for(module):
    return f"{module}Tests"


def test_modules_for(plugin):
    """Test modules owned by a plugin under Plugins/<plugin>/Tests/Source."""
    source = REPO / "Plugins" / plugin / "Tests" / "Source"
    if not source.is_dir():
        return []
    return sorted(m.name for m in source.iterdir() if (m / f"{m.name}.Build.cs").is_file())


def plugin_test_sources(plugin):
    return {module: REPO / "Plugins" / plugin / "Tests" / "Source" / module for module in test_modules_for(plugin)}


def test_filter_for(modules):
    # StartsWith: appends the dot, so AnimationDataTools. does not also select AnimationDataToolsEditor.
    return "+".join(f"StartsWith:{module}" for module in modules)


def stage_plugin(plugin, source_dir, stage_root):
    """Copies only the allowlisted entries of source_dir into stage_root/plugin."""
    allowed = allowlist_for(plugin)
    destination = stage_root / plugin
    if destination.exists():
        remove_tree(destination)
    destination.mkdir(parents=True)
    for entry in allowed:
        source = source_dir / entry
        if source.is_dir():
            shutil.copytree(source, destination / entry)
        elif source.is_file():
            shutil.copy2(source, destination / entry)
    check_tree_allowlist(destination, set(allowed), f"stage of {plugin}")
    return destination


def check_tree_allowlist(directory, allowed, label):
    unexpected = sorted(child.name for child in directory.iterdir() if child.name not in allowed)
    if unexpected:
        raise RunnerError(f"{label} contains entries outside the allowlist: {unexpected}")


def build_plugin(plugin, engine):
    stage = stage_plugin(plugin, REPO / "Plugins" / plugin, TMP / "stage")
    package = TMP / "pkg" / plugin
    if package.exists():
        remove_tree(package)
    runuat = engine / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat"
    command = [str(runuat), "BuildPlugin", f"-Plugin={stage / (plugin + '.uplugin')}",
               f"-Package={package}", "-TargetPlatforms=Win64", "-Rocket", "-Unversioned"]
    log_path = TMP / "logs" / f"buildplugin-{plugin}.log"
    log(f"[build] {plugin}: BuildPlugin -> {log_path}")
    code, _ = run_logged(command, log_path, timeout=3600)
    text = log_path.read_text(encoding="utf-8", errors="replace")
    warnings = sorted(set(re.findall(r"[^\s(]+\(\d+(?:,\d+)?\): warning [A-Z]+\d+:[^\r\n]*", text)))
    plugin_warnings = [w for w in warnings if f"{os.sep}Plugins{os.sep}{plugin}{os.sep}" in w or f"/Plugins/{plugin}/" in w]
    if code != 0:
        raise RunnerError(f"BuildPlugin failed for {plugin} (exit {code}); see {log_path}")
    if plugin_warnings:
        raise RunnerError(f"BuildPlugin produced warnings in {plugin} sources: {plugin_warnings[:5]}")
    check_tree_allowlist(package, set(allowlist_for(plugin)) | PACKAGE_EXTRA, f"package of {plugin}")
    descriptor_problems = package_descriptor_problems(package)
    if descriptor_problems:
        raise RunnerError(f"BuildPlugin stamped a packaged descriptor for {plugin}: {descriptor_problems}")
    package_links = package_link_problems(package)
    if package_links:
        raise RunnerError(f"Packaged documentation has broken links for {plugin}: {package_links}")
    if (package / "HostProject").exists() or (stage / "HostProject").exists():
        raise RunnerError(f"BuildPlugin left a host project inside the stage or package of {plugin}")
    log(f"[build] {plugin}: OK")
    return package


# ---------------------------------------------------------------------------------------------
# Host projects
# ---------------------------------------------------------------------------------------------

def write_host(host_dir, plugins, test_modules, plugin_sources, test_sources=None):
    """Generates a host project from Tests/Host. plugin_sources maps plugin name to the directory
    copied into the host's Plugins folder (a BuildPlugin package, or a source tree in dev mode).
    test_sources maps a test module to its source directory; SUPPORT_MODULE stays under Tests/Host."""
    if host_dir.exists():
        remove_tree(host_dir)
    (host_dir / "Source").mkdir(parents=True)
    test_modules = [SUPPORT_MODULE] + list(test_modules)
    descriptor = {
        "FileVersion": 3,
        "EngineAssociation": "",
        "Category": "",
        "Description": "Generated verification host. Do not edit; regenerate with Tests/run.py.",
        "Modules": [{"Name": module, "Type": "Editor", "LoadingPhase": "Default"} for module in test_modules],
        "Plugins": [{"Name": plugin, "Enabled": True} for plugin in plugins],
    }
    (host_dir / "Host.uproject").write_text(json.dumps(descriptor, indent=4), encoding="utf-8")
    target = (TESTS / "Host" / "HostEditor.Target.cs.in").read_text(encoding="utf-8")
    modules = ", ".join(f'"{module}"' for module in test_modules)
    (host_dir / "Source" / "HostEditor.Target.cs").write_text(target.replace("@MODULES@", modules), encoding="utf-8")
    test_sources = test_sources or {}
    for module in test_modules:
        source = TESTS / "Host" / "Source" / module if module == SUPPORT_MODULE else test_sources[module]
        shutil.copytree(source, host_dir / "Source" / module)
    for plugin, source in plugin_sources.items():
        destination = host_dir / "Plugins" / plugin
        shutil.copytree(source, destination,
                        ignore=shutil.ignore_patterns("Intermediate", "Binaries", "Tests") if source.parent.name == "Plugins" else None)
    # Each plugin owns its fixtures; a test that needs one fails on its own when it is missing.
    for plugin in plugins:
        fixtures = REPO / "Plugins" / plugin / "Tests" / "Fixtures"
        if fixtures.is_dir():
            shutil.copytree(fixtures, host_dir / "Fixtures", dirs_exist_ok=True)
    return host_dir / "Host.uproject"


def build_host(uproject, engine, label):
    build_bat = engine / "Engine" / "Build" / "BatchFiles" / "Build.bat"
    command = [str(build_bat), "HostEditor", "Win64", "Development", f"-Project={uproject}", "-WaitMutex", "-NoHotReloadFromIDE"]
    log_path = TMP / "logs" / f"hostbuild-{label}.log"
    log(f"[host] {label}: building editor target -> {log_path}")
    code, _ = run_logged(command, log_path, timeout=3600)
    if code != 0:
        raise RunnerError(f"Host build failed for {label} (exit {code}); see {log_path}")


def run_automation(uproject, engine, test_filter, label, timeout=1800):
    report_dir = TMP / "reports" / label
    if report_dir.exists():
        remove_tree(report_dir)
    report_dir.mkdir(parents=True)
    editor = engine / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
    command = [str(editor), str(uproject), f"-ExecCmds=Automation RunTests {test_filter};Quit",
               "-TestExit=Automation Test Queue Empty", f"-ReportExportPath={report_dir}",
               "-unattended", "-nullrhi", "-nosplash", "-NoSound", "-stdout", "-FullStdOutLogOutput",
               "-NoP4", "-nocore"]
    log_path = TMP / "logs" / f"automation-{label}.log"
    log(f"[test] {label}: Automation RunTests {test_filter} -> {log_path}")
    code, timed_out = run_logged(command, log_path, timeout=timeout)
    log(f"[test] {label}: editor {'timed out' if timed_out else f'exited with {code}'}")
    return report_dir, code, timed_out, log_path


def check_editor_exit(code, timed_out, label, log_path):
    """A nonzero editor exit fails the run even when a report was exported."""
    if timed_out:
        raise RunnerError(f"{label}: automation timed out; see {log_path}")
    if code != 0:
        raise RunnerError(f"{label}: editor exited with {code}; see {log_path}")


def read_report(report_dir):
    """Returns per-test states from an automation report. A missing report is a failure."""
    index = report_dir / "index.json"
    if not index.is_file():
        raise RunnerError(f"No automation report at {index}")
    data = json.loads(index.read_text(encoding="utf-8-sig"))
    tests = data.get("tests", [])
    states = {}
    for test in tests:
        states[test.get("fullTestPath", test.get("testDisplayName", "?"))] = test.get("state", "?")
    return states


def check_report(states, expected, label):
    """expected maps a test-path prefix (a module name) to its declared test count, or is None."""
    if not states:
        raise RunnerError(f"{label}: the report lists no tests")
    failed = sorted(name for name, state in states.items() if state not in ("Success",))
    if failed:
        raise RunnerError(f"{label}: tests not successful: {failed}")
    if expected is not None:
        for module, count in sorted(expected.items()):
            ran = sum(1 for name in states if name.startswith(module + "."))
            if ran != count:
                raise RunnerError(f"{label}: ran {ran} {module} tests, the declared count is {count}")
        if len(states) != sum(expected.values()):
            raise RunnerError(f"{label}: ran {len(states)} tests, the declared total is {sum(expected.values())}")
    log(f"[test] {label}: {len(states)} tests passed")


def expected_counts(plugin):
    return json.loads((REPO / "Plugins" / plugin / "Tests" / "expected-tests.json").read_text(encoding="utf-8"))


def test_plugin(plugin, engine, package=None):
    package = package or build_plugin(plugin, engine)
    modules = plugin_modules(plugin)
    test_modules = test_modules_for(plugin)
    host = TMP / "host" / plugin
    test_sources = {module: REPO / "Plugins" / plugin / "Tests" / "Source" / module for module in test_modules}
    uproject = write_host(host, [plugin], test_modules, {plugin: package}, test_sources)
    build_host(uproject, engine, plugin)
    report_dir, code, timed_out, log_path = run_automation(uproject, engine, test_filter_for(modules), plugin)
    declared = expected_counts(plugin)
    check_report(read_report(report_dir), {m: declared.get(m, 0) for m in modules}, plugin)
    check_editor_exit(code, timed_out, plugin, log_path)


def dev_host(engine, test_filter, plugins):
    """Builds one host with the selected plugins from source and their test modules, for fast
    iteration. Acceptance always uses test_plugin, which installs packages one plugin at a time."""
    host = TMP / "host" / "dev"
    test_modules = [test_module_for(m) for p in plugins for m in plugin_modules(p)]
    test_sources = {}
    for plugin in plugins:
        test_sources.update(plugin_test_sources(plugin))
    uproject = write_host(host, plugins, test_modules, {p: REPO / "Plugins" / p for p in plugins}, test_sources)
    build_host(uproject, engine, "dev")
    if test_filter:
        report_dir, code, timed_out, log_path = run_automation(uproject, engine, test_filter, "dev")
        states = read_report(report_dir)
        for name, state in sorted(states.items()):
            log(f"  {state:8} {name}")
        check_report(states, None, "dev")
        check_editor_exit(code, timed_out, "dev", log_path)


# ---------------------------------------------------------------------------------------------
# Guards
# ---------------------------------------------------------------------------------------------

def guarded_files():
    """Files the naming guard covers: everything under Plugins/, Tests/ and docs/, and the root
    README. This runner is excluded because it holds the guard's own lists."""
    roots = [REPO / "Plugins", REPO / "Tests", REPO / "docs"]
    files = [REPO / "README.md"]
    this_file = Path(__file__).resolve()
    for root in roots:
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in TEXT_SUFFIXES and path.resolve() != this_file:
                if any(part in ("Binaries", "Intermediate") for part in path.relative_to(REPO).parts):
                    continue
                files.append(path)
    return files


def naming_violations(path_name, text):
    """Applies the naming guard to one file. Returns a list of human-readable violations."""
    violations = []
    for identifier in RETIRED_IDENTIFIERS:
        if re.search(rf"\b{re.escape(identifier)}\b", text):
            violations.append(f"rule 1: retired identifier {identifier}")
    for file_name in RETIRED_FILE_NAMES:
        if file_name in text or Path(path_name).name == file_name:
            violations.append(f"rule 1: retired file name {file_name}")
    if Path(path_name).suffix.lower() in SOURCE_SUFFIXES:
        for name in RETIRED_DECLARATIONS:
            # An unqualified Name( declares, defines or calls our own function. Qualified and member
            # calls (FMath::Name(, Object->Name(, Object.Name() reach engine APIs and stay allowed.
            if re.search(rf"(?<![.:>\w]){re.escape(name)}\s*\(", text):
                violations.append(f"rule 2: unqualified retired name {name}(")
    if Path(path_name).suffix.lower() == ".ini" and REDIRECT_KEYS.search(text):
        violations.append("rule 3: redirect entry")
    return violations


def reset_audio_violations(text):
    violations = []
    if re.search(r"\bResetAudio\s*\(", text):
        violations.append("calls ResetAudio(")
    if re.search(r"OnSoundWaveProceduralUnderflow\s*\.\s*(Unbind|Clear)\b", text):
        violations.append("unbinds or clears OnSoundWaveProceduralUnderflow")
    return violations


def descriptor_violations(text):
    data = json.loads(text)
    value = data.get("EngineVersion", "")
    return [f'EngineVersion is "{value}"'] if value else []


def package_descriptor_problems(package_root):
    """Descriptors BuildPlugin produced must keep EngineVersion empty."""
    problems = []
    if not package_root.is_dir():
        return problems
    for descriptor in sorted(package_root.glob("*/*.uplugin")):
        try:
            text = descriptor.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as error:
            problems.append(f"{descriptor.relative_to(REPO)}: unreadable ({error})")
            continue
        try:
            problems += [f"{descriptor.relative_to(REPO)}: {v}" for v in descriptor_violations(text)]
        except json.JSONDecodeError as error:
            problems.append(f"{descriptor.relative_to(REPO)}: invalid JSON ({error})")
    return problems


def guard_naming():
    problems = []
    for path in guarded_files():
        text = path.read_text(encoding="utf-8", errors="replace")
        for violation in naming_violations(path.name, text):
            problems.append(f"{path.relative_to(REPO)}: {violation}")
    return problems


def guard_descriptors():
    problems = []
    for plugin in PLUGINS:
        descriptor = REPO / "Plugins" / plugin / f"{plugin}.uplugin"
        if not descriptor.is_file():
            problems.append(f"missing descriptor {descriptor.relative_to(REPO)}")
            continue
        problems += [f"{descriptor.relative_to(REPO)}: {v}" for v in descriptor_violations(descriptor.read_text(encoding="utf-8"))]
    return problems


def guard_layout():
    problems = []
    for plugin in PLUGINS:
        source_root = REPO / "Plugins" / plugin / "Source"
        if not source_root.is_dir():
            problems.append(f"{plugin}: no Source directory")
            continue
        for module in sorted(p for p in source_root.iterdir() if p.is_dir()):
            names = {child.name for child in module.iterdir()}
            if not {"Public", "Private"} <= names:
                problems.append(f"{module.relative_to(REPO)}: needs Public/ and Private/")
            if f"{module.name}.Build.cs" not in names:
                problems.append(f"{module.relative_to(REPO)}: missing {module.name}.Build.cs")
            extra = names - {"Public", "Private", f"{module.name}.Build.cs", "README.md", "docs"}
            if extra:
                problems.append(f"{module.relative_to(REPO)}: unexpected entries {sorted(extra)}")
    return problems


def license_problems(repo, plugins):
    root_license = (repo / "LICENSE").read_bytes()
    problems = []
    for plugin in plugins:
        plugin_license = repo / "Plugins" / plugin / "LICENSE"
        if not plugin_license.is_file() or plugin_license.read_bytes() != root_license:
            problems.append(f"Plugins/{plugin}/LICENSE is missing or differs from the root LICENSE")
    return problems


def guard_licenses():
    return license_problems(REPO, PLUGINS)


def guard_reset_audio():
    problems = []
    source = REPO / "Plugins" / "StreamingAudio" / "Source"
    for path in source.rglob("*"):
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
            problems += [f"{path.relative_to(REPO)}: {v}" for v in reset_audio_violations(path.read_text(encoding="utf-8", errors="replace"))]
    return problems


def plugin_names(repo):
    plugins_root = repo / "Plugins"
    return sorted(p.name for p in plugins_root.iterdir() if p.is_dir()) if plugins_root.is_dir() else []


def documentation_owners(repo):
    """Directories that own architecture, and directories that only route."""
    owners = [repo, repo / "Tests"]
    routers = [repo / "Plugins", repo / "Tests" / "Host", repo / "Tests" / "Host" / "Source"]
    for plugin in plugin_names(repo):
        owners.append(repo / "Plugins" / plugin)
        routers.append(repo / "Plugins" / plugin / "Source")
        routers.append(repo / "Plugins" / plugin / "Tests")
        routers.append(repo / "Plugins" / plugin / "Tests" / "Source")
        if (repo / "Plugins" / plugin / "Tests" / "Fixtures").is_dir():
            routers.append(repo / "Plugins" / plugin / "Tests" / "Fixtures")
        for module in plugin_modules(plugin, repo / "Plugins"):
            owners.append(repo / "Plugins" / plugin / "Source" / module)
        for module in test_modules_for_plugin(repo, plugin):
            routers.append(repo / "Plugins" / plugin / "Tests" / "Source" / module)
    return owners, routers


def test_modules_for_plugin(repo, plugin):
    source = repo / "Plugins" / plugin / "Tests" / "Source"
    return sorted(m.name for m in source.iterdir() if (m / f"{m.name}.Build.cs").is_file()) if source.is_dir() else []


def missing_owner_docs(repo):
    owners, routers = documentation_owners(repo)
    problems = []
    for owner in owners:
        problems += [f"{(owner / doc).relative_to(repo)}: missing" for doc in OWNER_DOCS if not (owner / doc).is_file()]
    problems += [f"{(router / 'README.md').relative_to(repo)}: missing" for router in routers if not (router / "README.md").is_file()]
    return problems


def markdown_files(repo):
    files = [repo / "README.md"] if (repo / "README.md").is_file() else []
    for root in (repo / "docs", repo / "Plugins", repo / "Tests"):
        if root.is_dir():
            files += [p for p in root.rglob("*.md")
                      if not any(part in ("Binaries", "Intermediate") for part in p.relative_to(repo).parts)]
    return files


def local_links(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    targets = []
    for target in MARKDOWN_LINK.findall(text):
        if re.match(r"^[a-z][a-z0-9+.-]*:", target, re.IGNORECASE) or target.startswith("#"):
            continue
        targets.append((target, (path.parent / target.split("#", 1)[0]).resolve()))
    return targets


def documentation_link_problems(repo):
    """Every relative link resolves, and every document is routed: a README.md from its parent
    directory's README.md, any other document from the README.md beside it."""
    problems = []
    files = markdown_files(repo)
    links = {path.resolve(): {resolved for _, resolved in local_links(path)} for path in files}
    for path in files:
        for target, resolved in local_links(path):
            if not resolved.exists():
                problems.append(f"{path.relative_to(repo)}: broken link {target}")
    for path in files:
        if path.resolve() == (repo / "README.md").resolve():
            continue
        router = path.parent.parent / "README.md" if path.name == "README.md" else path.parent / "README.md"
        if path.resolve() not in links.get(router.resolve(), set()):
            if not repository_link_routes(router, repo, path):
                problems.append(f"{path.relative_to(repo)}: not routed from {router.relative_to(repo)}")
    return problems


def repository_link_routes(router, repo, target):
    """A source-only document may be routed from an absolute repository URL in a packaged README."""
    if not router.is_file():
        return False
    relative = target.relative_to(repo).as_posix()
    for link in MARKDOWN_LINK.findall(router.read_text(encoding="utf-8", errors="replace")):
        if re.match(r"^https?://", link, re.IGNORECASE) and relative in link:
            return True
    return False


def package_link_problems(package_root):
    """Relative markdown links in a package must resolve inside that package."""
    problems = []
    if not package_root.is_dir():
        return problems
    root = package_root.resolve()
    for path in package_root.rglob("*.md"):
        for target, resolved in local_links(path):
            try:
                resolved.relative_to(root)
            except ValueError:
                problems.append(f"{path.relative_to(package_root)}: link escapes package {target}")
                continue
            if not resolved.exists():
                problems.append(f"{path.relative_to(package_root)}: broken link {target}")
    return problems


def missing_module_tests(repo):
    problems = []
    for plugin in plugin_names(repo):
        expected_path = repo / "Plugins" / plugin / "Tests" / "expected-tests.json"
        declared = json.loads(expected_path.read_text(encoding="utf-8")) if expected_path.is_file() else {}
        test_names = test_modules_for_plugin(repo, plugin)
        for module in plugin_modules(plugin, repo / "Plugins"):
            test_module = test_module_for(module)
            if test_module not in test_names:
                problems.append(f"{module}: no test module Plugins/{plugin}/Tests/Source/{test_module}")
            if declared.get(module, 0) <= 0:
                problems.append(f"{module}: expected-tests.json declares no tests")
        if not expected_path.is_file():
            problems.append(f"Plugins/{plugin}/Tests/expected-tests.json: missing")
    return problems


def scan_targets():
    listed = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=REPO,
                            capture_output=True, text=True, check=True).stdout.splitlines()
    targets = [REPO / name for name in listed if not name.startswith(("todo/", "tmp/"))]
    for extra in (TMP / "stage", TMP / "pkg"):
        if extra.is_dir():
            targets += [p for p in extra.rglob("*") if p.is_file()]
    return targets


def denylist_expression(lines):
    """Each non-comment line is a pattern: a line with a word boundary is a regex, any other
    line is a literal. Matching is case-insensitive."""
    literals = [line.strip() for line in lines if line.strip() and not line.startswith("#")]
    if not literals:
        raise RunnerError("the denylist is empty")
    patterns = [literal if "\\b" in literal else re.escape(literal) for literal in literals]
    try:
        return re.compile("|".join(f"(?:{pattern})" for pattern in patterns), re.IGNORECASE)
    except re.error as error:
        raise RunnerError(f"invalid denylist pattern: {error}")


def denylist_matches(text, expression):
    return expression.search(text) is not None


def history_excludes():
    """The retired public descriptor holds only the allowed existing attribution."""
    return [":(exclude)" + name for name in RETIRED_FILE_NAMES]


def guard_denylist(denylist_path, history_range):
    expression = denylist_expression(Path(denylist_path).read_text(encoding="utf-8").splitlines())
    problems = []
    targets = [path for path in scan_targets() if path.is_file()]
    for path in targets:
        data = path.read_bytes()
        # Text is Latin-1 here; Windows wide strings carry their letters as UTF-16 code units,
        # so a binary-only scan would miss them.
        views = [data.decode("latin-1"), data.decode("utf-16-le", errors="ignore"),
                 data.decode("utf-16-be", errors="ignore")]
        if not data.isascii():
            views.append(data.decode("utf-8", errors="ignore"))
        normalized = [text.replace("\\", "/") for text in views]
        if any(denylist_matches(text, expression) for text in views + normalized):
            problems.append(f"{path}: matches the denylist")
    history_checked = False
    if history_range:
        command = ["git", "log", "-p", "--format=", history_range, "--", "."] + history_excludes()
        history = subprocess.run(command, cwd=REPO, capture_output=True,
                                 text=True, encoding="utf-8", errors="replace", check=True).stdout
        history_checked = True
        if denylist_matches(history, expression):
            problems.append(f"history {history_range}: matches the denylist")
    log(f"[guards] denylist: scanned {len(targets)} files, UTF-16 views, "
        f"history={history_range if history_checked else 'none'}")
    return problems


def internal_record_problems(repo, tracked=None):
    """Internal investigation records must stay out of the index and publication."""
    if tracked is None:
        tracked = subprocess.run(["git", "ls-files", "--cached", "--", "todo", "tmp"], cwd=repo,
                                 capture_output=True, text=True, check=True).stdout.splitlines()
    return [f"{name}: internal investigation record is tracked or indexed" for name in tracked
            if name.startswith(("todo/", "tmp/"))]


def run_guards(denylist, history_range):
    results = {
        "naming": guard_naming(),
        "descriptors (EngineVersion empty)": guard_descriptors(),
        "packaged descriptors (EngineVersion empty)": package_descriptor_problems(TMP / "pkg"),
        "module layout (Public/Private)": guard_layout(),
        "LICENSE identity": guard_licenses(),
        "StreamingAudio queue ownership": guard_reset_audio(),
        "owner documents": missing_owner_docs(REPO),
        "document links and routes": documentation_link_problems(REPO),
        "packaged documentation links": package_link_problems(TMP / "pkg"),
        "module tests": missing_module_tests(REPO),
        "internal investigation records": internal_record_problems(REPO),
    }
    if denylist:
        results["denylist"] = guard_denylist(denylist, history_range)
    else:
        log("[guards] denylist: skipped (pass --denylist; the list lives outside this repository)")
    failed = False
    for name, problems in results.items():
        if problems:
            failed = True
            log(f"[guards] {name}: FAIL")
            for problem in problems:
                log(f"    {problem}")
        else:
            log(f"[guards] {name}: OK")
    if failed:
        raise RunnerError("guards failed")


def self_test():
    """Controls: each guard and report check must reject a planted violation and accept the
    legitimate case."""
    checks = []

    def expect(label, condition):
        checks.append((label, bool(condition)))

    expect("rule 1 rejects a retired type", naming_violations("A.cpp", "#include \"Core/System/CustomBPLibrary.h\""))
    expect("rule 1 rejects UCustomBPLibrary", naming_violations("A.cpp", "UCustomBPLibrary::Foo();"))
    expect("rule 2 rejects a retired declaration",
           naming_violations("A.h", "static FVector GetLockedAxis(const UPrimitiveComponent* C);"))
    expect("rule 2 accepts a member engine call", not naming_violations("A.cpp", "const FVector Axis = BodyInstance->GetLockedAxis();"))
    expect("rule 2 accepts a qualified engine call",
           not naming_violations("A.cpp", "const FVector P = FMath::RayPlaneIntersection(Origin, Direction, Plane);"))
    expect("rule 2 accepts a longer fresh name", not naming_violations("A.h", "static bool TryGetBodyLockedAxis(const UPrimitiveComponent* C, FVector& Out);"))
    expect("rule 3 rejects a redirect", naming_violations("DefaultEngine.ini", "[CoreRedirects]\n+ClassRedirects=(OldName=\"A\",NewName=\"B\")"))
    expect("descriptor check rejects a pinned version", descriptor_violations('{"EngineVersion": "5.8.0"}'))
    expect("descriptor check accepts an empty version", not descriptor_violations('{"EngineVersion": ""}'))
    expect("queue check rejects ResetAudio", reset_audio_violations("Wave->ResetAudio();"))
    expect("queue check rejects Unbind", reset_audio_violations("Wave->OnSoundWaveProceduralUnderflow.Unbind();"))
    expect("queue check accepts QueueAudio", not reset_audio_violations("Wave->QueueAudio(Data, Num);"))
    planted = denylist_expression(["# comment", "Private[Name"])
    expect("denylist finds a planted literal, ignoring case", denylist_matches("see private[name here", planted))
    expect("denylist matches literally, not as a pattern", not denylist_matches("PrivateNName", planted))
    boundary_terms = denylist_expression([r"\bterm\b"])
    expect("a boundary term matches a whole word", denylist_matches("a term here", boundary_terms))
    expect("a boundary term does not match a longer identifier", not denylist_matches("terminal", boundary_terms))
    try:
        denylist_expression(["# only a comment"])
        expect("an empty denylist fails", False)
    except RunnerError:
        expect("an empty denylist fails", True)

    scratch = TMP / "guard-self-test"
    if scratch.exists():
        remove_tree(scratch)
    plugin_dir = scratch / "src" / "Sample"
    for entry in ("Source/Sample/Public", "tmp", "todo", "Intermediate", "Binaries"):
        (plugin_dir / entry).mkdir(parents=True, exist_ok=True)
    (plugin_dir / "Sample.uplugin").write_text("{}", encoding="utf-8")
    (plugin_dir / "tmp" / "marker.txt").write_text("marker", encoding="utf-8")
    (plugin_dir / "todo" / "marker.txt").write_text("marker", encoding="utf-8")
    staged = stage_plugin("Sample", plugin_dir, scratch / "stage")
    expect("stage excludes tmp/ and todo/ markers",
           not (staged / "tmp").exists() and not (staged / "todo").exists() and (staged / "Sample.uplugin").exists())
    package_dir = scratch / "pkg" / "Sample"
    package_dir.mkdir(parents=True)
    (package_dir / "Sample.uplugin").write_text('{"EngineVersion": "5.8.0"}', encoding="utf-8")
    expect("packaged descriptor check rejects a stamped version", package_descriptor_problems(scratch / "pkg"))
    (package_dir / "Sample.uplugin").write_text('{"EngineVersion": ""}', encoding="utf-8")
    expect("packaged descriptor check accepts an empty version", not package_descriptor_problems(scratch / "pkg"))
    (package_dir / "README.md").write_text("[tests](Tests/README.md)", encoding="utf-8")
    expect("packaged link check rejects a source-only relative link", package_link_problems(scratch / "pkg"))
    (package_dir / "docs").mkdir()
    (package_dir / "docs" / "README.md").write_text("", encoding="utf-8")
    (package_dir / "README.md").write_text(
        "[tests](https://github.com/example/repo/blob/develop/Tests/README.md) [docs](docs/README.md)", encoding="utf-8")
    expect("packaged link check accepts repository and in-package links", not package_link_problems(scratch / "pkg"))

    repo = scratch / "repo"
    module = repo / "Plugins" / "Sample" / "Source" / "Sample"
    module.mkdir(parents=True)
    (module / "Sample.Build.cs").write_text("", encoding="utf-8")
    for owner in (repo, repo / "Tests", repo / "Plugins" / "Sample", module):
        for doc in OWNER_DOCS:
            (owner / doc).parent.mkdir(parents=True, exist_ok=True)
            (owner / doc).write_text("", encoding="utf-8")
    for router in ("Plugins", "Plugins/Sample/Source", "Plugins/Sample/Tests", "Plugins/Sample/Tests/Source",
                   "Plugins/Sample/Tests/Source/SampleTests", "Tests/Host", "Tests/Host/Source"):
        (repo / router).mkdir(parents=True, exist_ok=True)
        (repo / router / "README.md").write_text("", encoding="utf-8")
    expect("owner-document check accepts a complete tree", not missing_owner_docs(repo))
    (module / "docs" / "architecture" / "structure.md").unlink()
    expect("owner-document check rejects a module without structure.md", missing_owner_docs(repo))
    (repo / "README.md").write_text("[plugins](Plugins/README.md) [gone](Missing.md)", encoding="utf-8")
    link_problems = documentation_link_problems(repo)
    routed = str(Path("Plugins") / "README.md") + ":"
    unrouted = str(Path("Plugins") / "Sample" / "README.md") + ":"
    expect("link check rejects a broken link", any("broken link Missing.md" in p for p in link_problems))
    expect("route check accepts a routed README", not any(p.startswith(routed) for p in link_problems))
    expect("route check rejects an unrouted README", any(p.startswith(unrouted) for p in link_problems))
    test_build = repo / "Plugins" / "Sample" / "Tests" / "Source" / "SampleTests" / "SampleTests.Build.cs"
    test_build.parent.mkdir(parents=True, exist_ok=True)
    test_build.write_text("", encoding="utf-8")
    expected = repo / "Plugins" / "Sample" / "Tests" / "expected-tests.json"
    expected.write_text('{"Sample": 1}', encoding="utf-8")
    expect("module-test check accepts a declared test module", not missing_module_tests(repo))
    expected.write_text("{}", encoding="utf-8")
    expect("module-test check rejects zero declared tests", missing_module_tests(repo))
    test_build.unlink()
    expect("module-test check rejects a module without a test module", missing_module_tests(repo))
    expect("internal-record check rejects a tracked TODO",
           internal_record_problems(repo, ["todo/record.md"]))
    expect("internal-record check accepts a normal source file",
           not internal_record_problems(repo, ["Plugins/Sample/Source/Sample/Sample.Build.cs"]))
    (repo / "LICENSE").write_text("MIT License\n", encoding="utf-8")
    (repo / "Plugins" / "Sample" / "LICENSE").write_text("MIT License\n", encoding="utf-8")
    expect("licence check accepts an identical copy", not license_problems(repo, ["Sample"]))
    (repo / "Plugins" / "Sample" / "LICENSE").write_text("MIT License \n", encoding="utf-8")
    expect("licence check rejects a copy that differs by one byte", license_problems(repo, ["Sample"]))

    report = scratch / "report"
    report.mkdir()
    try:
        read_report(report)
        expect("a missing report fails", False)
    except RunnerError:
        expect("a missing report fails", True)
    try:
        check_report({}, None, "empty")
        expect("an empty test run fails", False)
    except RunnerError:
        expect("an empty test run fails", True)
    try:
        check_report({"A.One": "Success"}, {"A": 2}, "count")
        expect("a count mismatch fails", False)
    except RunnerError:
        expect("a count mismatch fails", True)
    try:
        check_report({"A.One": "Success", "AB.One": "Success"}, {"A": 1}, "extra")
        expect("an undeclared extra test fails", False)
    except RunnerError:
        expect("an undeclared extra test fails", True)
    report_ok = scratch / "report-ok"
    report_ok.mkdir()
    (report_ok / "index.json").write_text(json.dumps({"tests": [{"fullTestPath": "A.One", "state": "Success"}]}), encoding="utf-8")
    expect("a successful report is accepted", read_report(report_ok) == {"A.One": "Success"})
    try:
        check_editor_exit(1, False, "control", scratch / "control.log")
        expect("a failed editor process fails a successful report", False)
    except RunnerError:
        expect("a failed editor process fails a successful report", True)
    check_editor_exit(0, False, "control", scratch / "control.log")
    expect("a clean editor exit is accepted", True)
    remove_tree(scratch)

    failed = [label for label, ok in checks if not ok]
    for label, ok in checks:
        log(f"[self-test] {'OK  ' if ok else 'FAIL'} {label}")
    if failed:
        raise RunnerError(f"self-test failed: {failed}")


# ---------------------------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=["self-test", "guards", "build", "test", "dev", "all"])
    parser.add_argument("plugins", nargs="*", help="plugins to build or test (default: all)")
    parser.add_argument("--engine", help="engine root (default: UE_ENGINE_ROOT)")
    parser.add_argument("--denylist", help="denylist file kept outside this repository")
    parser.add_argument("--history", help="git revision range whose patches the denylist also scans")
    parser.add_argument("--filter", default="", help="automation filter for the dev command")
    args = parser.parse_args()
    started = time.time()
    try:
        selected = args.plugins or PLUGINS
        unknown = [p for p in selected if p not in PLUGINS]
        if unknown:
            raise RunnerError(f"unknown plugins: {unknown}")
        if args.command == "self-test":
            self_test()
        elif args.command == "guards":
            run_guards(args.denylist, args.history)
        else:
            engine = resolve_engine(args.engine)
            log(f"[engine] {engine} ({engine_version(engine)})")
            if args.command == "build":
                for plugin in selected:
                    build_plugin(plugin, engine)
            elif args.command == "test":
                for plugin in selected:
                    test_plugin(plugin, engine)
            elif args.command == "dev":
                dev_host(engine, args.filter, selected)
            elif args.command == "all":
                self_test()
                for plugin in selected:
                    test_plugin(plugin, engine)
                # Packages exist now, so the final guards also scan the staged and packaged files.
                run_guards(args.denylist, args.history)
    except RunnerError as error:
        log(f"FAILED: {error}")
        return 1
    log(f"Done in {time.time() - started:.0f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
