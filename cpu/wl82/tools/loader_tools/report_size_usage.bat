
@rem 放到下载目录中
@rem 会把 sdk.elf 中使用的空间大小，
@rem           按地址排序，输出到 symbol_tbl.txt 中

set OBJSIZEDUMP=C:\JL\pi32\bin\llvm-objsizedump.exe
set ELF=sdk.elf
set OUTPUT=symbol_tbl.txt

%OBJSIZEDUMP% -lite -skip-zero -enable-dbg-info %ELF% | sort > %OUTPUT%
