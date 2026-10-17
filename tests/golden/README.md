# GPU golden data

These are 64×64 RGBA8 readbacks from the synthetic fixture in `gpu_tests.cpp`.
Views: 0 = Lit, 3 = Normal, 4 = Depth, 5 = Lighting, 6 = Shadow.

The GPU test compares with PSNR ≥ 28 dB. To regenerate on the current driver:

1. Create `update-golden.flag` in the CTest working directory (usually the build root).
2. Run `imrelighter_gpu_tests`.
3. Remove the flag and rerun to verify the comparison.
