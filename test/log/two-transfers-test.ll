; TODO: fix pid in log name
; XFAIL: *
; RUN: %projbindir/tern-instr < %s -S -o %t1
; RUN: llc -o %t1.s %t1-record.ll
; RUN: g++ -o %t1 %t1.s %ternruntime -lpthread -lrt
; RUN: rm -rf %t1.outdir
; RUN: env TERN_OPTIONS=log_type=bin:output_dir=%t1.outdir ./%t1
; RUN: %projbindir/logprint -bc %t1-analysis.ll %t1.outdir/tid-0.bin -r -v | FileCheck %s.out


; ModuleID = 'two-transfers-test.ll'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64"
target triple = "x86_64-unknown-linux-gnu"

@i = common global i32 0                          ; <i32*> [#uses=1]

define i32 @foo() nounwind {
entry:
  %0 = load i32* @i, align 4
  %1 = add i32 %0, 10
  store i32 %1, i32* @i, align 4
  br label %return

return:                                           ; preds = %entry
  %retval1 = load i32* @i                    ; <i32> [#uses=1]
  ret i32 %retval1
}

define i32 @main() nounwind {
entry:
  br label %bb

bb:                                               ; preds = %bb, %entry
  %0 = call i32 @foo() nounwind                   ; <i32> [#uses=0]
  %1 = icmp sle i32 %0, 100
  br i1 %1, label %bb, label %bb1

bb1:                                              ; preds = %bb
  br label %return

return:                                           ; preds = %bb1
  ret i32 0
}
