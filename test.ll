define i32 @test(i32 %a, i32 %b) {
entry:
  %1 = add i32 %a, %b
  %2 = icmp eq i32 %a, %b
  br i1 %2, label %branch1, label %exit

branch1:
  %3 = add i32 %b, %a
  br label %exit

exit:
  %ret = phi i32 [ %1, %entry ], [ %3, %branch1 ]
  ret i32 %ret
}


; define i32 @test1(i32 %a, i32 %b) {
; entry:
;   %1 = add i32 %a, %b
;   %2 = add i32 %a, %b
;   %3 = add i32 %1, %2
;   ret i32 %3
; }
