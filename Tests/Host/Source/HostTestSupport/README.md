# HostTestSupport

Helper module every generated host carries. `Public/BlueprintCompileCheck.h` builds a transient
Blueprint whose graph calls given functions, wired as a Blueprint author would wire them, and
compiles it; test modules use it to prove their plugin's nodes compile in a graph.
