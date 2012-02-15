; TODO: fix pid in log name
; XFAIL: *
; RUN: %projbindir/tern-instr < %s -S -o %t1
; RUN: llc -o %t1.s %t1-record.ll
; RUN: g++ -o %t1 %t1.s %ternruntime -lpthread -lrt
; RUN: ./%t1
; RUN: rm -rf %t1.outdir
; RUN: env TERN_OPTIONS=log_type=bin:output_dir=%t1.outdir ./%t1
; RUN: %projbindir/logprint -bc %t1-analysis.ll %t1.outdir/tid-0.bin -r -v | FileCheck %s.out

; ModuleID = 'two-transfers-test.ll'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64"
target triple = "x86_64-unknown-linux-gnu"

%struct.timeval = type { i64, i64 }

define i32 @main() nounwind {
entry:
  %FullTime = alloca %struct.timeval, align 8
  %0 = call i32 @gettimeofday(%struct.timeval* noalias %FullTime, i8* noalias null) nounwind
  %1 = getelementptr inbounds %struct.timeval* %FullTime, i64 0, i32 1
  %2 = load i64* %1, align 8
  br label %return

return:
  ret i32 %0
}

declare i32 @gettimeofday(%struct.timeval* noalias, i8* noalias) nounwind
