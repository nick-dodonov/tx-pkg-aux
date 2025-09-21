"""Repository rule for conditional lwlog source selection"""

def _lwlog_source_impl(repository_ctx):
    """Implementation of lwlog_source repository rule"""
    
    # Check if source config is enabled
    use_source = repository_ctx.attr.use_source
    local_path = repository_ctx.attr.local_path
    
    if use_source and repository_ctx.path(local_path).exists:
        # Use local source path
        repository_ctx.symlink(repository_ctx.path(local_path), ".")
        
        # Create a marker file to indicate we're using local source
        repository_ctx.file("LWLOG_SOURCE_MODE", "local")
    else:
        # Create empty repository - will use registry version
        repository_ctx.file("LWLOG_SOURCE_MODE", "registry")
        repository_ctx.file("BUILD.bazel", "# Empty - using registry lwlog")

lwlog_source = repository_rule(
    implementation = _lwlog_source_impl,
    attrs = {
        "use_source": attr.bool(default = False, doc = "Whether to use local source"),
        "local_path": attr.string(doc = "Path to local lwlog source"),
    },
    local = True,  # This repository should be re-evaluated on every build
)