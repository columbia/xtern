; RUN: %projbindir/tern-instr < %s -S -o %t1
; RUN: llc -o %t1.s %t1-record.ll
; RUN: %gxx -o %t1 %t1.s %ternruntime -lpthread -lrt
; RUN: ./%t1
; : %projbindir/logprint -bc %t1-analysis.ll tern-log-tid-0 -r -v > /dev/null

; ModuleID = 'invoke-test.cpp'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64"
target triple = "x86_64-unknown-linux-gnu"

%struct.__fundamental_type_info_pseudo = type { %struct.__type_info_pseudo }
%struct.__type_info_pseudo = type { i8*, i8* }

@_ZTIi = external constant %struct.__fundamental_type_info_pseudo ; <%struct.__fundamental_type_info_pseudo*> [#uses=1]

define i32 @_Z3fooi(i32 %x) {
entry:
  %x_addr = alloca i32                            ; <i32*> [#uses=4]
  %retval = alloca i32                            ; <i32*> [#uses=2]
  %0 = alloca i8*                                 ; <i8**> [#uses=3]
  %1 = alloca i32                                 ; <i32*> [#uses=2]
  %"alloca point" = bitcast i32 0 to i32          ; <i32> [#uses=0]
  store i32 %x, i32* %x_addr
  %2 = load i32* %x_addr, align 4                 ; <i32> [#uses=1]
  %3 = icmp sge i32 %2, 0                         ; <i1> [#uses=1]
  br i1 %3, label %bb, label %bb1

bb:                                               ; preds = %entry
  %4 = load i32* %x_addr, align 4                 ; <i32> [#uses=1]
  store i32 %4, i32* %1, align 4
  %5 = load i32* %1, align 4                      ; <i32> [#uses=1]
  store i32 %5, i32* %retval, align 4
  br label %return

bb1:                                              ; preds = %entry
  %6 = call i8* @__cxa_allocate_exception(i64 4) nounwind ; <i8*> [#uses=1]
  store i8* %6, i8** %0, align 8
  %7 = load i8** %0, align 8                      ; <i8*> [#uses=1]
  %8 = bitcast i8* %7 to i32*                     ; <i32*> [#uses=1]
  %9 = load i32* %x_addr, align 4                 ; <i32> [#uses=1]
  store i32 %9, i32* %8, align 4
  %10 = load i8** %0, align 8                     ; <i8*> [#uses=1]
  call void @__cxa_throw(i8* %10, i8* bitcast (%struct.__fundamental_type_info_pseudo* @_ZTIi to i8*), void (i8*)* null) noreturn
  unreachable

return:                                           ; preds = %bb
  %retval2 = load i32* %retval                    ; <i32> [#uses=1]
  ret i32 %retval2
}

declare i8* @__cxa_allocate_exception(i64) nounwind

declare void @__cxa_throw(i8*, i8*, void (i8*)*) noreturn

define i32 @main() {
entry:
  %retval = alloca i32                            ; <i32*> [#uses=2]
  %0 = alloca i32                                 ; <i32*> [#uses=2]
  %1 = alloca i32*                                ; <i32**> [#uses=2]
  %x = alloca i32                                 ; <i32*> [#uses=1]
  %eh_exception = alloca i8*                      ; <i8**> [#uses=4]
  %eh_selector = alloca i32                       ; <i32*> [#uses=2]
  %"alloca point" = bitcast i32 0 to i32          ; <i32> [#uses=0]
  %2 = invoke i32 @_Z3fooi(i32 10)
          to label %invcont unwind label %lpad    ; <i32> [#uses=0]

invcont:                                          ; preds = %entry
  %3 = invoke i32 @getpid()
          to label %invcont1 unwind label %lpad   ; <i32> [#uses=0]

invcont1:                                         ; preds = %invcont
  br label %bb2

bb:                                               ; preds = %ppad
  %eh_value = load i8** %eh_exception             ; <i8*> [#uses=1]
  %4 = call i8* @__cxa_begin_catch(i8* %eh_value) nounwind ; <i8*> [#uses=1]
  %5 = bitcast i8* %4 to i32*                     ; <i32*> [#uses=1]
  store i32* %5, i32** %1, align 8
  %6 = load i32** %1, align 8                     ; <i32*> [#uses=1]
  %7 = load i32* %6, align 4                      ; <i32> [#uses=1]
  store i32 %7, i32* %x, align 4
  call void @__cxa_end_catch()
  br label %bb2

bb2:                                              ; preds = %bb, %invcont1
  store i32 0, i32* %0, align 4
  %8 = load i32* %0, align 4                      ; <i32> [#uses=1]
  store i32 %8, i32* %retval, align 4
  br label %return

return:                                           ; preds = %bb2
  %retval3 = load i32* %retval                    ; <i32> [#uses=1]
  ret i32 %retval3

lpad:                                             ; preds = %invcont, %entry
  %eh_ptr = call i8* @llvm.eh.exception()         ; <i8*> [#uses=1]
  store i8* %eh_ptr, i8** %eh_exception
  %eh_ptr4 = load i8** %eh_exception              ; <i8*> [#uses=1]
  %eh_select = call i32 (i8*, i8*, ...)* @llvm.eh.selector(i8* %eh_ptr4, i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*), i8* bitcast (%struct.__fundamental_type_info_pseudo* @_ZTIi to i8*), i8* null) ; <i32> [#uses=1]
  store i32 %eh_select, i32* %eh_selector
  br label %ppad

ppad:                                             ; preds = %lpad
  %eh_typeid = call i32 @llvm.eh.typeid.for(i8* bitcast (%struct.__fundamental_type_info_pseudo* @_ZTIi to i8*)) ; <i32> [#uses=1]
  %9 = load i32* %eh_selector                     ; <i32> [#uses=1]
  %10 = icmp eq i32 %9, %eh_typeid                ; <i1> [#uses=1]
  br i1 %10, label %bb, label %nocatch

nocatch:                                          ; preds = %ppad
  br label %Unwind

Unwind:                                           ; preds = %nocatch
  %eh_ptr5 = load i8** %eh_exception              ; <i8*> [#uses=1]
  call void @_Unwind_Resume_or_Rethrow(i8* %eh_ptr5)
  unreachable
}

declare i32 @getpid()

declare i8* @__cxa_begin_catch(i8*) nounwind

declare i8* @llvm.eh.exception() nounwind readonly

declare i32 @llvm.eh.selector(i8*, i8*, ...) nounwind

declare i32 @llvm.eh.typeid.for(i8*) nounwind

declare void @__cxa_end_catch()

declare i32 @__gxx_personality_v0(...)

declare void @_Unwind_Resume_or_Rethrow(i8*)
