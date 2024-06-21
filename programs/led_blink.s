movk w1, #0x1000
movk x0, #0x3f20, lsl #16
str w1, [x0]
movz w1, #0x10

movz x2, #0x3f20, lsl #16
movk w2, #0x1c

movz x3, #0x3f20, lsl #16
movk w3, #0x40

movz x6, #0x0100, lsl #16

eor x10, x10, x10
loop:
  str w1, [x2]
  str w10, [x3]

eor x5, x5, x5
delay:
  add x5, x5, #1
  cmp x5, x6
  b.ne delay

str w1, [x3]
str w10, [x2]


eor x5, x5, x5
delay2:
  add x5, x5, #1
  cmp x5, x6
  b.ne delay2

b loop
