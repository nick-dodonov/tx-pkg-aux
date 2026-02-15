# Hybrid Bash+Batch In One File

Allows to run `sh_binary` on Windows even when specific build platform is selected _(e.g. `--platforms=@emsdk//:platform_wasm`)_.
In that case .exe wrapper isn't produced by rule implementation.

Reference:
1. https://stackoverflow.com/questions/61802896/is-it-possible-to-make-a-bat-bash-hybrid
    - **Abandoned**: it gives error on 1st line in batch mode w/o `cls`
2. https://danq.me/2022/06/13/bash-batch/
    - **Current solution**: works only with implicit `bash` _(`sh` gives "`@goto': not a valid identifier")_
3. https://stackoverflow.com/questions/29966924/cross-platform-command-line-script-e-g-bat-and-sh
    - **TODO**: try this way (maybe its better with `sh`)
