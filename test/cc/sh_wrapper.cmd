@goto(){
# Shell script here
$(dirname "$0")/sh_wrapper.sh
exec "$@"
}
@goto $@
exit

:(){
@echo off
:: Batch script here
call "%~dp0sh_wrapper.bat"
%*
