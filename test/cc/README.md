# Hybrid Bash+Batch In One File
Allows to run `sh_binary` on Windows even when specific build platform is selected _(e.g. `--platforms=@emsdk//:platform_wasm`)_.
In that case .exe wrapper isn't produced by rule implementation.

Reference:
- https://stackoverflow.com/questions/61802896/is-it-possible-to-make-a-bat-bash-hybrid
- https://stackoverflow.com/questions/29966924/cross-platform-command-line-script-e-g-bat-and-sh
- https://danq.me/2022/06/13/bash-batch/


## TODO: also try another solutions
> Maybe to get rid of "'#!' is not recognized as an internal or external command, operable program or batch file."
```
@goto(){
  # Linux code here
  uname -o
}

@goto $@
exit

:(){
@echo off
rem Windows script here
echo %OS%
```
