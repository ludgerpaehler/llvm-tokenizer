define void @floats(ptr %h, ptr %f, ptr %d, ptr %q, ptr %x, ptr %p) {
  store half      0xH3C00,                      ptr %h
  store float     1.5,                          ptr %f
  store double    3.14159265358979,             ptr %d
  store fp128     0xL00000000000000004000900000000000, ptr %q
  store x86_fp80  0xK4000C90FDAA22168C000,      ptr %x
  store ppc_fp128 0xM40090000000000000000000000000000, ptr %p
  ret void
}
