; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@kern.name = private unnamed_addr constant [220 x i8] c"_cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_0\00"
@kern.name.1 = private unnamed_addr constant [220 x i8] c"_cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_1\00"
@_cuabi_fatbin_ptr = private constant [8568 x i8] c"P\EDU\BA\01\00\10\00h!\00\00\00\00\00\00\02\00\01\01@\00\00\00(!\00\00\00\00\00\00\00\00\00\00\00\00\00\00\07\00\01\00P\00\00\00\00\00\00\00\00\00\00\00\11\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\7FELF\02\01\013\07\00\00\00\00\00\00\00\02\00\BE\00{\00\00\00\00\00\00\00\00\00\00\00\80 \00\00\00\00\00\00\00\1D\00\00\00\00\00\00P\05P\00@\008\00\03\00@\00\0E\00\01\00\00.shstrtab\00.strtab\00.symtab\00.symtab_shndx\00.nv.uft.entry\00.nv.info\00.text._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_1\00.nv.info._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_1\00.nv.shared._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_1\00.nv.constant0._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_1\00.text._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_0\00.nv.info._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_0\00.nv.shared._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_0\00.nv.constant0._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_0\00.debug_frame\00.rel.debug_frame\00.rela.debug_frame\00.nv.rel.action\00\00.shstrtab\00.strtab\00.symtab\00.symtab_shndx\00.nv.uft.entry\00.nv.info\00_cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_1\00.text._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_1\00.nv.info._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_1\00.nv.shared._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_1\00.nv.constant0._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_1\00_param\00_cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_0\00.text._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_0\00.nv.info._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_0\00.nv.shared._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_0\00.nv.constant0._cuabi_kern__ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE_0\00.debug_frame\00.rel.debug_frame\00.rela.debug_frame\00.nv.rel.action\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\1C\01\00\00\03\00\0C\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\CA\03\00\00\03\00\0A\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\97\05\00\00\03\00\0D\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00E\08\00\00\03\00\0B\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00/\09\00\00\03\00\04\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00_\09\00\00\03\00\08\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00@\00\00\00\12\10\0C\00\00\00\00\00\00\00\00\00\80\02\00\00\00\00\00\00\BB\04\00\00\12\10\0D\00\00\00\00\00\00\00\00\00\80\02\00\00\00\00\00\00\FF\FF\FF\FF$\00\00\00\00\00\00\00\FF\FF\FF\FF\FF\FF\FF\FF\03\00\04|\FF\FF\FF\FF\0F\0C\81\80\80(\00\08\FF\81\80(\08\81\80\80(\00\00\00\FF\FF\FF\FF4\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\80\02\00\00\00\00\00\00\04\04\00\00\00\04\18\00\00\00\0C\81\80\80(\00\04D\00\00\00\00\00\00\00\00\00\00\FF\FF\FF\FF$\00\00\00\00\00\00\00\FF\FF\FF\FF\FF\FF\FF\FF\03\00\04|\FF\FF\FF\FF\0F\0C\81\80\80(\00\08\FF\81\80(\08\81\80\80(\00\00\00\FF\FF\FF\FF4\00\00\00\00\00\00\00p\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\80\02\00\00\00\00\00\00\04\04\00\00\00\04\18\00\00\00\0C\81\80\80(\00\04H\00\00\00\00\00\00\00\00\00\00\04/\08\00\08\00\00\00\0E\00\00\00\04#\08\00\08\00\00\00\00\00\00\00\04\12\08\00\08\00\00\00\00\00\00\00\04\11\08\00\08\00\00\00\00\00\00\00\04/\08\00\07\00\00\00\0E\00\00\00\04#\08\00\07\00\00\00\00\00\00\00\04\12\08\00\07\00\00\00\00\00\00\00\04\11\08\00\07\00\00\00\00\00\00\00\047\04\00{\00\00\00\015\00\00\04\0A\08\00\02\00\00\00`\018\00\03\198\00\04\17\0C\00\00\00\00\00\06\000\00\00\F0!\00\04\17\0C\00\00\00\00\00\05\00(\00\00\F0!\00\04\17\0C\00\00\00\00\00\04\00 \00\00\F0!\00\04\17\0C\00\00\00\00\00\03\00\18\00\00\F0!\00\04\17\0C\00\00\00\00\00\02\00\10\00\00\F0!\00\04\17\0C\00\00\00\00\00\01\00\08\00\00\F0!\00\04\17\0C\00\00\00\00\00\00\00\00\00\00\F0!\00\03\1B\FF\00\04\1C\08\00`\00\00\00\80\01\00\00\047\04\00{\00\00\00\015\00\00\04\0A\08\00\04\00\00\00`\010\00\03\190\00\04\17\0C\00\00\00\00\00\05\00(\00\00\F0!\00\04\17\0C\00\00\00\00\00\04\00 \00\00\F0!\00\04\17\0C\00\00\00\00\00\03\00\18\00\00\F0!\00\04\17\0C\00\00\00\00\00\02\00\10\00\00\F0!\00\04\17\0C\00\00\00\00\00\01\00\08\00\00\F0!\00\04\17\0C\00\00\00\00\00\00\00\00\00\00\F0!\00\03\1B\FF\00\04\1C\08\00`\00\00\00\90\01\00\00s\00\00\00\00\00\00\00\00\00\00\11%\00\056\B4\00\00\00\00\00\00\00\02\00\00\00\08\00\00\00D\00\00\00\00\00\00\00\02\00\00\00\07\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00$v\01\FF\00\0A\00\00\FF\00\8E\07\00\C4\0F\00\19y\00\00\00\00\00\00\00!\00\00\00(\0E\00\19y\03\00\00\00\00\00\00%\00\00\00$\0E\00$z\00\03\00\00\00\00\00\02\8E\07\00\CA\1F\00\0Cz\00\00\00X\00\00p`\F0\03\00\C8\0F\00\0Cz\00\FF\00Y\00\00\00a\F0\03\00\DA\0F\00M\09\00\00\00\00\00\00\00\00\80\03\00\EA\0F\00$x\0A\00\08\00\00\00\FF\00\8E\07\00\E2\0F\00\19x\00\FF\1D\00\00\00\00\16\01\00\00\E2\0F\00\B9z\04\00\00F\00\00\00\0A\00\00\00\C6\0F\00\10z\02\0A\00^\00\00\FF\E0\F1\07\00\E4\0F\04\10z\04\0A\00`\00\00\FF\E0\F3\07\00\E4\0F\00\10z\03\00\00_\00\00\FF\E4\7F\00\00\E4\0F\04\10z\05\00\00a\00\00\FF\E4\FF\00\00\C8\0F\00\81y\02\02\04\00\00\00\00\1B\1E\0C\00\A8\0E\00\81y\04\04\04\00\00\00\00\1B\1E\0C\00\A2\0E\00\10z\08\0A\00b\00\00\FF\E0\F3\07\00\E4\0F\04\10z\0A\0A\00d\00\00\FF\E0\F5\07\00\E4\0F\00\10z\09\00\00c\00\00\FF\E4\FF\00\00\E4\0F\04\10z\0B\00\00e\00\00\FF\E4\7F\01\00\C4\0F\00\10r\06\02\04\00\00\00\FF\E0\F1\07\00\CAO\00$x\07\03\01\00\00\00\05\06\0E\00\00\CA\0F\00\86y\00\08\06\00\00\00\04\1B\10\0C\00\E8\0F\00\86y\00\0A\FF\00\00\00\04\1B\10\0C\00\E2\0F\00My\00\00\00\00\00\00\00\00\80\03\00\EA\0F\00Gy\00\00\F0\FF\FF\FF\FF\FF\83\03\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00$v\01\FF\00\0A\00\00\FF\00\8E\07\00\C4\0F\00\19y\00\00\00\00\00\00\00!\00\00\00(\0E\00\19y\03\00\00\00\00\00\00%\00\00\00$\0E\00$z\00\03\00\00\00\00\00\02\8E\07\00\CA\1F\00\0Cz\00\00\00X\00\00p`\F0\03\00\C8\0F\00\0Cz\00\FF\00Y\00\00\00a\F0\03\00\DA\0F\00M\09\00\00\00\00\00\00\00\00\80\03\00\EA\0F\00$x\08\00\08\00\00\00\FF\00\8E\07\00\E2\0F\00\19x\00\FF\1D\00\00\00\00\16\01\00\00\E2\0F\00\B9z\04\00\00F\00\00\00\0A\00\00\00\C6\0F\00\10z\02\08\00^\00\00\FF\E0\F1\07\00\C8\0F\00\10z\03\00\00_\00\00\FF\E4\7F\00\00\E4\0F\00\10z\04\08\00`\00\00\FF\E0\F1\07\00\C8\0F\00\81y\02\02\04\00\00\00\00\1B\1E\0C\00\A2\0E\00\10z\05\00\00a\00\00\FF\E4\7F\00\00\CC\0F\00\81y\04\04\04\00\00\00\00\1B\1E\0C\00\E2\0E\00\10z\08\08\00b\00\00\FF\E0\F3\07\00\C8\0F\00\10z\09\00\00c\00\00\FF\E4\FF\00\00\E4\0F\00\0Cx\00\02\05\00\00\00p@\F0\03\00\C8O\00\0Cr\00\03\FF\00\00\00\00C\F0\03\00\C8\0F\00\07x\07\02\05\00\00\00\00\00\00\00\00\E4\0F\00\07r\0B\03\FF\00\00\00\00\00\00\00\00\E4\0F\00\10r\06\04\07\00\00\00\FF\E0\F1\07\00\CA\8F\00$x\07\05\01\00\00\00\0B\06\0E\00\00\CA\0F\00\86y\00\08\06\00\00\00\04\1B\10\0C\00\E2\0F\00My\00\00\00\00\00\00\00\00\80\03\00\EA\0F\00Gy\00\00\F0\FF\FF\FF\FF\FF\83\03\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\18y\00\00\00\00\00\00\00\00\00\00\00\C0\0F\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\01\00\00\00\03\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00@\00\00\00\00\00\00\00\AF\07\00\00\00\00\00\00\00\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\0B\00\00\00\03\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\EF\07\00\00\00\00\00\00n\09\00\00\00\00\00\00\00\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\13\00\00\00\02\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00`\11\00\00\00\00\00\00\D8\00\00\00\00\00\00\00\02\00\00\00\07\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00\00\00\00\00p\07\00\00\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\008\12\00\00\00\00\00\00\E0\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00\00\00\00\007\00\00\00\00\00\00p\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\18\13\00\00\00\00\00\00`\00\00\00\00\00\00\00\03\00\00\00\00\00\00\00\04\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\22\01\00\00\00\00\00p\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00x\13\00\00\00\00\00\00\9C\00\00\00\00\00\00\00\03\00\00\00\0C\00\00\00\04\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\BA\04\00\00\00\00\00p\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\14\14\00\00\00\00\00\00\8C\00\00\00\00\00\00\00\03\00\00\00\0D\00\00\00\04\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\A0\07\00\00\0B\00\00p\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\A0\14\00\00\00\00\00\00\10\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00}\07\00\00\09\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\B0\14\00\00\00\00\00\00 \00\00\00\00\00\00\00\03\00\00\00\04\00\00\00\08\00\00\00\00\00\00\00\10\00\00\00\00\00\00\00\EE\02\00\00\01\00\00\00\02\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\D0\14\00\00\00\00\00\00\98\01\00\00\00\00\00\00\00\00\00\00\0C\00\00\00\04\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\86\06\00\00\01\00\00\00\02\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00h\16\00\00\00\00\00\00\90\01\00\00\00\00\00\00\00\00\00\00\0D\00\00\00\04\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00@\00\00\00\01\00\00\00\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\18\00\00\00\00\00\00\80\02\00\00\00\00\00\00\03\00\00\00\07\00\00\0E\80\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\D8\03\00\00\01\00\00\00\06\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\80\1A\00\00\00\00\00\00\80\02\00\00\00\00\00\00\03\00\00\00\08\00\00\0E\80\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\06\00\00\00\05\00\00\00\80 \00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\A8\00\00\00\00\00\00\00\A8\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00\01\00\00\00\05\00\00\00\D0\14\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\000\08\00\00\00\00\00\000\08\00\00\00\00\00\00\08\00\00\00\00\00\00\00\01\00\00\00\05\00\00\00\80 \00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\A8\00\00\00\00\00\00\00\A8\00\00\00\00\00\00\00\08\00\00\00\00\00\00\00"
@_cuabi_wrapper = internal constant { i32, i32, ptr, ptr } { i32 1180844977, i32 1, ptr @_cuabi_fatbin_ptr, ptr null }, align 8
@_cuabi.fbhand = internal global ptr null, align 8
@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65536, ptr @_cuabi.ctor._cuabiLLVMDialectModule, ptr null }]

; Function Attrs: alwaysinline
define { ptr, ptr, i64, [1 x i64], [1 x i64] } @__convert_to_memref_1xi64({ ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %0) local_unnamed_addr #0 {
  %2 = extractvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %0, 0
  %3 = extractvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %0, 4
  %4 = extractvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %0, 5
  %5 = extractvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %0, 6
  %6 = tail call ptr @nmrtCreateAllocToken()
  store ptr %2, ptr %6, align 8
  %7 = insertvalue { ptr, ptr, i64, [1 x i64], [1 x i64] } undef, ptr %6, 0
  %8 = insertvalue { ptr, ptr, i64, [1 x i64], [1 x i64] } %7, ptr %3, 1
  %9 = insertvalue { ptr, ptr, i64, [1 x i64], [1 x i64] } %8, i64 0, 2
  %10 = insertvalue { ptr, ptr, i64, [1 x i64], [1 x i64] } %9, [1 x i64] %4, 3
  %11 = extractvalue [1 x i64] %5, 0
  %12 = sdiv i64 %11, 8
  %13 = insertvalue [1 x i64] undef, i64 %12, 0
  %14 = insertvalue { ptr, ptr, i64, [1 x i64], [1 x i64] } %10, [1 x i64] %13, 4
  ret { ptr, ptr, i64, [1 x i64], [1 x i64] } %14
}

define noundef i32 @_ZN8__main__7foo_241B106c8tJTC_2fWQAlzW1yBDkop6GEOEUMEOYSPGuIQMViAQ3iQ8IbKQIMbwoOGNoQDDWwQR1NHAS3lQ9XgSucwm4pgLNTQs80DZTPd3JzMIk0AE10FixedArrayIxLi1E1C7mutable7alignedE10FixedArrayIxLi1E1C7mutable7alignedE(ptr nocapture writeonly %0, ptr nocapture readnone %1, ptr %2, ptr nocapture readnone %3, i64 %4, i64 %5, ptr nocapture readonly %6, i64 %7, i64 %8, ptr %9, ptr nocapture readnone %10, i64 %11, i64 %12, ptr nocapture readonly %13, i64 %14, i64 %15) local_unnamed_addr #1 {
  %17 = alloca [7 x ptr], align 8
  %18 = alloca ptr, align 8
  store ptr null, ptr %18, align 8
  %19 = alloca i64, align 8
  %20 = alloca i64, align 8
  %21 = alloca i64, align 8
  %22 = alloca ptr, align 8
  %23 = alloca ptr, align 8
  %24 = alloca ptr, align 8
  %25 = alloca ptr, align 8
  %26 = alloca [6 x ptr], align 8
  %27 = alloca ptr, align 8
  store ptr null, ptr %27, align 8
  %28 = alloca i64, align 8
  %29 = alloca i64, align 8
  %30 = alloca i64, align 8
  %31 = alloca ptr, align 8
  %32 = alloca ptr, align 8
  %33 = alloca ptr, align 8
  %syncreg18 = tail call token @llvm.syncregion.start()
  %syncreg = tail call token @llvm.syncregion.start()
  %34 = tail call ptr @nmrtCreateAllocToken()
  store ptr %2, ptr %34, align 8
  %35 = tail call ptr @nmrtCreateAllocToken()
  store ptr %9, ptr %35, align 8
  %.idx = shl i64 %7, 3
  %36 = tail call ptr @__kitcuda_mem_alloc_managed_numba(i64 %.idx, i32 32)
  %37 = getelementptr { i64, ptr, ptr, ptr, i64, ptr }, ptr %36, i64 0, i32 3
  %38 = load ptr, ptr %37, align 8
  %39 = tail call ptr @nmrtCreateAllocToken()
  store ptr %36, ptr %39, align 8
  %40 = tail call ptr @__kitcuda_mem_alloc_managed_numba(i64 %.idx, i32 32)
  %41 = getelementptr { i64, ptr, ptr, ptr, i64, ptr }, ptr %40, i64 0, i32 3
  %42 = load ptr, ptr %41, align 8
  %43 = tail call ptr @nmrtCreateAllocToken()
  store ptr %40, ptr %43, align 8
  %44 = icmp sgt i64 %7, 0
  br i1 %44, label %.lr.ph.preheader, label %._crit_edge

.lr.ph.preheader:                                 ; preds = %16
  %45 = call i64 @llvm.tapir.loop.grainsize.i64(i64 %7)
  br label %46

46:                                               ; preds = %.lr.ph.preheader
  store i64 %7, ptr %19, align 8
  %47 = getelementptr inbounds [7 x ptr], ptr %17, i32 0, i32 0
  store ptr %19, ptr %47, align 8
  store i64 0, ptr %20, align 8
  %48 = getelementptr inbounds [7 x ptr], ptr %17, i32 0, i32 1
  store ptr %20, ptr %48, align 8
  store i64 %45, ptr %21, align 8
  %49 = getelementptr inbounds [7 x ptr], ptr %17, i32 0, i32 2
  store ptr %21, ptr %49, align 8
  store ptr %6, ptr %22, align 8
  %50 = getelementptr inbounds [7 x ptr], ptr %17, i32 0, i32 3
  store ptr %22, ptr %50, align 8
  %51 = load ptr, ptr %18, align 8
  %52 = call ptr @__kitcuda_mem_gpu_prefetch(ptr %6, ptr %51)
  store ptr %52, ptr %18, align 8
  store ptr %13, ptr %23, align 8
  %53 = getelementptr inbounds [7 x ptr], ptr %17, i32 0, i32 4
  store ptr %23, ptr %53, align 8
  %54 = load ptr, ptr %18, align 8
  %55 = call ptr @__kitcuda_mem_gpu_prefetch(ptr %13, ptr %54)
  store ptr %38, ptr %24, align 8
  %56 = getelementptr inbounds [7 x ptr], ptr %17, i32 0, i32 5
  store ptr %24, ptr %56, align 8
  %57 = load ptr, ptr %18, align 8
  %58 = call ptr @__kitcuda_mem_gpu_prefetch(ptr %38, ptr %57)
  store ptr %42, ptr %25, align 8
  %59 = getelementptr inbounds [7 x ptr], ptr %17, i32 0, i32 6
  store ptr %25, ptr %59, align 8
  %60 = load ptr, ptr %18, align 8
  %61 = call ptr @__kitcuda_mem_gpu_prefetch(ptr %42, ptr %60)
  %62 = getelementptr inbounds [7 x ptr], ptr %17, i32 0, i32 0
  %63 = load ptr, ptr @_cuabi_fatbin_ptr, align 8
  %64 = alloca { i64, i64, i64 }, align 8
  store { i64, i64, i64 } { i64 4, i64 0, i64 5 }, ptr %64, align 8
  %65 = load ptr, ptr %18, align 8
  %_cubin.fatbin = bitcast ptr @_cuabi_fatbin_ptr to ptr
  %66 = call ptr @__kitcuda_launch_kernel(ptr %_cubin.fatbin, ptr @kern.name.1, ptr %62, i64 %7, i32 0, ptr %64, ptr %65)
  store ptr %66, ptr %18, align 8
  br label %newexit19

.lr.ph:                                           ; preds = %latch17
  detach within %syncreg18, label %body16, label %latch17

body16:                                           ; preds = %.lr.ph
  %67 = getelementptr i64, ptr %6, i64 %74
  %68 = load i64, ptr %67, align 8
  %69 = getelementptr i64, ptr %13, i64 %74
  %70 = load i64, ptr %69, align 8
  %71 = add i64 %70, %68
  %72 = getelementptr i64, ptr %38, i64 %74
  store i64 %71, ptr %72, align 8
  %73 = getelementptr i64, ptr %42, i64 %74
  store i64 0, ptr %73, align 8
  reattach within %syncreg18, label %latch17

latch17:                                          ; preds = %body16, %.lr.ph
  %74 = add nuw nsw i64 %74, 1
  %exitcond.not = icmp eq i64 %74, %7
  br i1 %exitcond.not, label %newexit19, label %.lr.ph, !llvm.loop !4

.lr.ph14:                                         ; preds = %latch
  detach within %syncreg, label %body, label %latch

body:                                             ; preds = %.lr.ph14
  %75 = getelementptr i64, ptr %38, i64 %82
  %76 = load i64, ptr %75, align 8
  %77 = tail call i64 @llvm.smax.i64(i64 %76, i64 5)
  %78 = getelementptr i64, ptr %6, i64 %82
  %79 = load i64, ptr %78, align 8
  %80 = add i64 %77, %79
  %81 = getelementptr i64, ptr %42, i64 %82
  store i64 %80, ptr %81, align 8
  reattach within %syncreg, label %latch

latch:                                            ; preds = %body, %.lr.ph14
  %82 = add nuw nsw i64 %82, 1
  %exitcond15.not = icmp eq i64 %82, %7
  br i1 %exitcond15.not, label %newexit, label %.lr.ph14, !llvm.loop !7

._crit_edge:                                      ; preds = %newexit, %16
  %custreamh = load ptr, ptr %18, align 8
  call void @__kitcuda_sync_thread_stream(ptr %custreamh)
  %83 = load ptr, ptr %39, align 8
  %84 = atomicrmw sub ptr %83, i64 1 seq_cst, align 8
  %85 = icmp eq i64 %84, 1
  br i1 %85, label %86, label %NRT_decref.exit

86:                                               ; preds = %._crit_edge
  tail call void @NRT_MemInfo_call_dtor(ptr %83)
  br label %NRT_decref.exit

NRT_decref.exit:                                  ; preds = %._crit_edge, %86
  tail call void @nmrtDestroyAllocToken(ptr nonnull %39)
  %87 = load ptr, ptr %43, align 8
  tail call void @nmrtDestroyAllocToken(ptr nonnull %43)
  store ptr %87, ptr %0, align 8
  %.repack2 = getelementptr inbounds { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] }, ptr %0, i64 0, i32 1
  store ptr null, ptr %.repack2, align 8
  %.repack4 = getelementptr inbounds { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] }, ptr %0, i64 0, i32 2
  store i64 %7, ptr %.repack4, align 8
  %.repack6 = getelementptr inbounds { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] }, ptr %0, i64 0, i32 3
  store i64 8, ptr %.repack6, align 8
  %.repack8 = getelementptr inbounds { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] }, ptr %0, i64 0, i32 4
  store ptr %42, ptr %.repack8, align 8
  %.repack10 = getelementptr inbounds { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] }, ptr %0, i64 0, i32 5
  store i64 %7, ptr %.repack10, align 8
  %.repack12 = getelementptr inbounds { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] }, ptr %0, i64 0, i32 6
  store i64 8, ptr %.repack12, align 8
  ret i32 0

newexit:                                          ; preds = %89, %latch
  sync within %syncreg, label %._crit_edge

newexit19:                                        ; preds = %46, %latch17
  sync within %syncreg18, label %newexit19.split

newexit19.split:                                  ; preds = %newexit19
  call void @__kitcuda_sync_thread_stream(ptr null)
  %88 = call i64 @llvm.tapir.loop.grainsize.i64(i64 %7)
  br label %89

89:                                               ; preds = %newexit19.split
  store i64 %7, ptr %28, align 8
  %90 = getelementptr inbounds [6 x ptr], ptr %26, i32 0, i32 0
  store ptr %28, ptr %90, align 8
  store i64 0, ptr %29, align 8
  %91 = getelementptr inbounds [6 x ptr], ptr %26, i32 0, i32 1
  store ptr %29, ptr %91, align 8
  store i64 %88, ptr %30, align 8
  %92 = getelementptr inbounds [6 x ptr], ptr %26, i32 0, i32 2
  store ptr %30, ptr %92, align 8
  store ptr %38, ptr %31, align 8
  %93 = getelementptr inbounds [6 x ptr], ptr %26, i32 0, i32 3
  store ptr %31, ptr %93, align 8
  %94 = load ptr, ptr %27, align 8
  %95 = call ptr @__kitcuda_mem_gpu_prefetch(ptr %38, ptr %94)
  store ptr %95, ptr %27, align 8
  store ptr %6, ptr %32, align 8
  %96 = getelementptr inbounds [6 x ptr], ptr %26, i32 0, i32 4
  store ptr %32, ptr %96, align 8
  %97 = load ptr, ptr %27, align 8
  %98 = call ptr @__kitcuda_mem_gpu_prefetch(ptr %6, ptr %97)
  store ptr %42, ptr %33, align 8
  %99 = getelementptr inbounds [6 x ptr], ptr %26, i32 0, i32 5
  store ptr %33, ptr %99, align 8
  %100 = load ptr, ptr %27, align 8
  %101 = call ptr @__kitcuda_mem_gpu_prefetch(ptr %42, ptr %100)
  %102 = getelementptr inbounds [6 x ptr], ptr %26, i32 0, i32 0
  %103 = load ptr, ptr @_cuabi_fatbin_ptr, align 8
  %104 = alloca { i64, i64, i64 }, align 8
  store { i64, i64, i64 } { i64 3, i64 0, i64 5 }, ptr %104, align 8
  %105 = load ptr, ptr %27, align 8
  %_cubin.fatbin22 = bitcast ptr @_cuabi_fatbin_ptr to ptr
  %106 = call ptr @__kitcuda_launch_kernel(ptr %_cubin.fatbin22, ptr @kern.name, ptr %102, i64 %7, i32 0, ptr %104, ptr %105)
  store ptr %106, ptr %27, align 8
  br label %newexit
}

; Function Attrs: alwaysinline
define { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } @__convert_from_memref_1xi64({ ptr, ptr, i64, [1 x i64], [1 x i64] } %0) local_unnamed_addr #0 {
  %2 = extractvalue { ptr, ptr, i64, [1 x i64], [1 x i64] } %0, 0
  %3 = extractvalue { ptr, ptr, i64, [1 x i64], [1 x i64] } %0, 1
  %4 = extractvalue { ptr, ptr, i64, [1 x i64], [1 x i64] } %0, 2
  %5 = extractvalue { ptr, ptr, i64, [1 x i64], [1 x i64] } %0, 3
  %6 = extractvalue { ptr, ptr, i64, [1 x i64], [1 x i64] } %0, 4
  %7 = load ptr, ptr %2, align 8
  tail call void @nmrtDestroyAllocToken(ptr nonnull %2)
  %8 = getelementptr i64, ptr %3, i64 %4
  %9 = extractvalue [1 x i64] %5, 0
  %10 = insertvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } undef, ptr %7, 0
  %11 = insertvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %10, ptr null, 1
  %12 = insertvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %11, i64 %9, 2
  %13 = insertvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %12, i64 8, 3
  %14 = insertvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %13, ptr %8, 4
  %15 = insertvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %14, [1 x i64] %5, 5
  %16 = extractvalue [1 x i64] %6, 0
  %17 = shl i64 %16, 3
  %18 = insertvalue [1 x i64] undef, i64 %17, 0
  %19 = insertvalue { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %15, [1 x i64] %18, 6
  ret { ptr, ptr, i64, i64, ptr, [1 x i64], [1 x i64] } %19
}

declare ptr @nmrtCreateAllocToken() local_unnamed_addr #1

declare void @nmrtDestroyAllocToken(ptr) local_unnamed_addr #1

define void @NRT_decref(ptr %0) local_unnamed_addr #1 {
  %2 = atomicrmw sub ptr %0, i64 1 seq_cst, align 8
  %3 = icmp eq i64 %2, 1
  br i1 %3, label %4, label %common.ret

common.ret:                                       ; preds = %1, %4
  ret void

4:                                                ; preds = %1
  tail call void @NRT_MemInfo_call_dtor(ptr %0)
  br label %common.ret
}

declare void @NRT_MemInfo_call_dtor(ptr) local_unnamed_addr #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.smax.i64(i64, i64) #2

declare ptr @__kitcuda_mem_alloc_managed_numba(i64, i32) local_unnamed_addr

; Function Attrs: mustprogress nounwind willreturn memory(argmem: readwrite)
declare token @llvm.syncregion.start() #3

declare ptr @__kitcuda_launch_kernel(ptr, ptr, ptr, i64, i32, ptr, ptr)

declare ptr @__kitcuda_mem_gpu_prefetch(ptr, ptr)

declare i64 @__kitcuda_get_global_symbol(ptr, ptr)

declare void @__kitcuda_memcpy_symbol_to_device(i32, i64, i64)

; Function Attrs: nounwind speculatable willreturn memory(none)
declare i64 @llvm.tapir.loop.grainsize.i64(i64) #4

declare void @__kitcuda_sync_thread_stream(ptr)

declare void @__kitcuda_memcpy_sym_to_device(ptr, i64, i64)

define internal void @_cuabi.ctor._cuabiLLVMDialectModule(ptr %0) {
entry:
  call void @__kitcuda_set_default_threads_per_blk(i32 256)
  call void @__kitcuda_initialize()
  call void @__kitcuda_use_occupancy_launch(i8 1)
  %1 = call ptr @__cudaRegisterFatBinary(ptr @_cuabi_wrapper)
  store ptr %1, ptr @_cuabi.fbhand, align 8
  %_cuabi.fbhand.ptr = load ptr, ptr @_cuabi.fbhand, align 8
  call void @__cudaRegisterFatBinaryEnd(ptr %1)
  %2 = call i32 @atexit(ptr @_cuabi.dtor)
  ret void
}

declare void @__kitcuda_set_default_threads_per_blk(i32)

declare void @__kitcuda_initialize()

declare void @__kitcuda_use_occupancy_launch(i8)

declare ptr @__cudaRegisterFatBinary(ptr)

declare void @__cudaRegisterFatBinaryEnd(ptr)

declare void @__cudaUnregisterFatBinary(ptr)

define internal void @_cuabi.dtor(ptr %0) {
entry:
  %1 = load ptr, ptr @_cuabi.fbhand, align 8
  call void @__cudaUnregisterFatBinary(ptr %1)
  call void @__kitcuda_destroy()
  ret void
}

declare void @__kitcuda_destroy()

declare i32 @atexit(ptr)

attributes #0 = { alwaysinline "target-cpu"="haswell" "target-features"="-prfchw,-cldemote,+avx,+aes,+sahf,+pclmul,-xop,+crc32,-xsaves,-avx512fp16,-usermsr,-sm4,+sse4.1,-avx512ifma,+xsave,-avx512pf,+sse4.2,-tsxldtrk,-ptwrite,-widekl,-sm3,+invpcid,+64bit,-xsavec,-avx10.1-512,-avx512vpopcntdq,+cmov,-avx512vp2intersect,-avx512cd,+movbe,-avxvnniint8,-avx512er,-amx-int8,-kl,-avx10.1-256,-sha512,-avxvnni,-rtm,-adx,+avx2,-hreset,-movdiri,-serialize,-vpclmulqdq,-avx512vl,-uintr,-clflushopt,-raoint,-cmpccxadd,+bmi,-amx-tile,+sse,-gfni,-avxvnniint16,-amx-fp16,+xsaveopt,+rdrnd,-avx512f,-amx-bf16,-avx512bf16,-avx512vnni,+cx8,-avx512bw,+sse3,-pku,+fsgsbase,-clzero,-mwaitx,-lwp,+lzcnt,-sha,-movdir64b,-wbnoinvd,-enqcmd,-prefetchwt1,-avxneconvert,-tbm,-pconfig,-amx-complex,+ssse3,+cx16,+bmi2,+fma,+popcnt,-avxifma,+f16c,-avx512bitalg,-rdpru,-clwb,+mmx,+sse2,-rdseed,-avx512vbmi2,-prefetchi,-rdpid,-fma4,-avx512vbmi,-shstk,-vaes,-waitpkg,-sgx,+fxsr,-avx512dq,-sse4a" }
attributes #1 = { "target-cpu"="haswell" "target-features"="-prfchw,-cldemote,+avx,+aes,+sahf,+pclmul,-xop,+crc32,-xsaves,-avx512fp16,-usermsr,-sm4,+sse4.1,-avx512ifma,+xsave,-avx512pf,+sse4.2,-tsxldtrk,-ptwrite,-widekl,-sm3,+invpcid,+64bit,-xsavec,-avx10.1-512,-avx512vpopcntdq,+cmov,-avx512vp2intersect,-avx512cd,+movbe,-avxvnniint8,-avx512er,-amx-int8,-kl,-avx10.1-256,-sha512,-avxvnni,-rtm,-adx,+avx2,-hreset,-movdiri,-serialize,-vpclmulqdq,-avx512vl,-uintr,-clflushopt,-raoint,-cmpccxadd,+bmi,-amx-tile,+sse,-gfni,-avxvnniint16,-amx-fp16,+xsaveopt,+rdrnd,-avx512f,-amx-bf16,-avx512bf16,-avx512vnni,+cx8,-avx512bw,+sse3,-pku,+fsgsbase,-clzero,-mwaitx,-lwp,+lzcnt,-sha,-movdir64b,-wbnoinvd,-enqcmd,-prefetchwt1,-avxneconvert,-tbm,-pconfig,-amx-complex,+ssse3,+cx16,+bmi2,+fma,+popcnt,-avxifma,+f16c,-avx512bitalg,-rdpru,-clwb,+mmx,+sse2,-rdseed,-avx512vbmi2,-prefetchi,-rdpid,-fma4,-avx512vbmi,-shstk,-vaes,-waitpkg,-sgx,+fxsr,-avx512dq,-sse4a" }
attributes #2 = { mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none) "target-cpu"="haswell" "target-features"="-prfchw,-cldemote,+avx,+aes,+sahf,+pclmul,-xop,+crc32,-xsaves,-avx512fp16,-usermsr,-sm4,+sse4.1,-avx512ifma,+xsave,-avx512pf,+sse4.2,-tsxldtrk,-ptwrite,-widekl,-sm3,+invpcid,+64bit,-xsavec,-avx10.1-512,-avx512vpopcntdq,+cmov,-avx512vp2intersect,-avx512cd,+movbe,-avxvnniint8,-avx512er,-amx-int8,-kl,-avx10.1-256,-sha512,-avxvnni,-rtm,-adx,+avx2,-hreset,-movdiri,-serialize,-vpclmulqdq,-avx512vl,-uintr,-clflushopt,-raoint,-cmpccxadd,+bmi,-amx-tile,+sse,-gfni,-avxvnniint16,-amx-fp16,+xsaveopt,+rdrnd,-avx512f,-amx-bf16,-avx512bf16,-avx512vnni,+cx8,-avx512bw,+sse3,-pku,+fsgsbase,-clzero,-mwaitx,-lwp,+lzcnt,-sha,-movdir64b,-wbnoinvd,-enqcmd,-prefetchwt1,-avxneconvert,-tbm,-pconfig,-amx-complex,+ssse3,+cx16,+bmi2,+fma,+popcnt,-avxifma,+f16c,-avx512bitalg,-rdpru,-clwb,+mmx,+sse2,-rdseed,-avx512vbmi2,-prefetchi,-rdpid,-fma4,-avx512vbmi,-shstk,-vaes,-waitpkg,-sgx,+fxsr,-avx512dq,-sse4a" }
attributes #3 = { mustprogress nounwind willreturn memory(argmem: readwrite) }
attributes #4 = { nounwind speculatable willreturn memory(none) }

!llvm.module.flags = !{!0, !1, !2, !3}

!0 = !{i32 1, !"Code Model", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"direct-access-external-data", i32 1}
!4 = distinct !{!4, !5, !6}
!5 = !{!"tapir.loop.target", i32 2}
!6 = !{!"tapir.loop.spawn.strategy", i32 0}
!7 = distinct !{!7, !5, !6}
