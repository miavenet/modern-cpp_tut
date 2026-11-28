# Lesson 06 — Practice Bank, part 5 of 11 (optional): What the Build Really Kept

**Time: about 27 minutes (estimate)** for 2 cards (12 + 15 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: machine code per template instantiation, and devirtualization.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB1 · Apply
Q: How much machine code does one more distinct instantiation of a template cost in the program you ship? `tsize.cpp` instantiates `work<I>` (fill, `std::sort`, `std::lower_bound` and erase on a `std::vector<Rec<I>>`) for NTYPES distinct record types of the same layout. For NTYPES = 1, 2, 4, 8, 16: compile to an object file, then link it with `tmain.cpp`, once with the default link and once with `-Wl,-no_deduplicate`. Read section sizes with `size -m`. Before running, predict: does the per-type slope of `__text` in the object file match the slope in the linked binary? One loop does all 15 builds (each size prints the object, the default binary, then the no-dedup binary):
   ```sh
   P=modules/07-architecture/lab/practice
   for n in 1 2 4 8 16; do clang++ -std=c++20 -O2 -DNDEBUG -DNTYPES=$n -c $P/tsize.cpp -o "${TMPDIR:-/tmp}/t$n.o" && clang++ -std=c++20 -O2 $P/tmain.cpp "${TMPDIR:-/tmp}/t$n.o" -o "${TMPDIR:-/tmp}/b$n" && clang++ -std=c++20 -O2 $P/tmain.cpp "${TMPDIR:-/tmp}/t$n.o" -Wl,-no_deduplicate -o "${TMPDIR:-/tmp}/b${n}nd" && echo "NTYPES=$n" && size -m "${TMPDIR:-/tmp}/t$n.o" "${TMPDIR:-/tmp}/b$n" "${TMPDIR:-/tmp}/b${n}nd"; done
   ```
   Answer format: bytes per extra type for the object file, the default link and the no-dedup link, then one line on what to measure.
<details><summary>Answer</summary>
Key:

  - Object files: `__text` grows linearly, about 5.5 KiB per extra type (5752 B at 1 type, 89956 B at 16: about 5614 B/type).
  - Linked binary, default link: about 0.5 KiB per type (5772 to 13176 B: about 494 B/type). The linker's deduplication pass merged most of the per-type copies. With `-Wl,-no_deduplicate` (an Apple linker flag; `size -m` is the Mach-O `size`, so these commands are macOS-only) the binary is back at about 5614 B/type.
  - So measure the final linked binary, with the link flags you ship; object-file sizes can overstate the cost many times over. The slope, not one size, is the per-type cost.
  - Also list the non-code sections: `__gcc_except_tab` grows 24 B/type (40 to 400 B) in both. `__compact_unwind` (352 to 3744 B in the objects, in the `__LD` segment) is linker input: the binary has no such section and carries `__unwind_info` instead (176 to 360 B by default, 176 to 720 B without dedup).
  - The record types here share one layout, which favours merging. Types whose code differs were not measured, so measure your own program; do not assume this ratio.
  - Holds for / re-check: Apple clang 21, libc++, ld64-954.16 (Mach-O), `-O2 -DNDEBUG`, arm64, this one template. These are byte counts, not timings. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), ELF with GNU ld (bfd; linker version not recorded): the default link did not merge (about 2.3 KB (2306-2378 B) per type in object and binary), and `-fuse-ld=gold -Wl,--icf=all` gave about 22 B per type. ELF objects keep the instantiations in COMDAT `.text.*` sections, so sum those. Re-check: link the real program both ways and compare `size` output.
</details>

### Card M7-PB8 · Apply
Q: `devirt/shape.h` and `devirt/shape.cpp` hold an interface `Shape` with one virtual function `area` and its only implementation `Square final`. `devirt/use.cpp` has two noinline functions with the same loop: `sum_iface(Shape&, ...)` and `sum_leaf(Square&, ...)`. Build four ways and look for `blr` (indirect branch) vs `bl` (direct call) in `objdump -d`: (A) no LTO, one object file; (B) add `-flto` and link both files; (C) `-flto -fwhole-program-vtables -fvisibility=hidden`; (D) as (A) but with `final` removed (edit a copy of the three files). Predict each before you look.
   ```sh
   D=modules/07-architecture/lab/practice/devirt
   clang++ -std=c++20 -O2 -c $D/use.cpp -o "${TMPDIR:-/tmp}/use.o" && objdump -d -r "${TMPDIR:-/tmp}/use.o"
   clang++ -std=c++20 -O2 -flto $D/use.cpp $D/shape.cpp -o "${TMPDIR:-/tmp}/B" && objdump -d "${TMPDIR:-/tmp}/B"
   clang++ -std=c++20 -O2 -flto -fwhole-program-vtables -fvisibility=hidden $D/use.cpp $D/shape.cpp -o "${TMPDIR:-/tmp}/C" && objdump -d "${TMPDIR:-/tmp}/C"
   cp -R $D "${TMPDIR:-/tmp}/devirtD" && sed -i '' 's/Square final/Square/' "${TMPDIR:-/tmp}/devirtD/shape.h" && clang++ -std=c++20 -O2 -c "${TMPDIR:-/tmp}/devirtD/use.cpp" -o "${TMPDIR:-/tmp}/useD.o" && objdump -d -r "${TMPDIR:-/tmp}/useD.o"
   ```
   Hint: `objdump -d` may label the first function in an object with a local label such as `ltmp0` instead of its name; `objdump -t` gives each symbol's address, so you can match them.
   Answer format: one line per build (A to D), then the lesson in one line.
<details><summary>Answer</summary>
Key:

  - (A) No LTO: `sum_iface` uses `blr`. `sum_leaf` uses a direct `bl` to `Square::area`: the static type is `Square`, and `final` means no class can override it, so this one file is enough.
  - (D) Remove `final` and `sum_leaf` goes back to `blr`: a subclass in another file could override `area`.
  - (B) LTO alone: `sum_iface` still has four `blr` (the loop is unrolled). Inside the noinline function, `s` is a parameter, so its dynamic type is not known there. With this clang, LTO alone did not use the fact that `Square` is the only implementation.
  - (C) Whole-program vtables with hidden visibility: no `blr` remains; the two functions became identical and were folded, so `main` calls `sum_iface` twice. Clang's docs say these whole-program vtable features "use whole-program information" and apply only to classes with hidden LTO visibility, which `-fvisibility=hidden` gives. It works from the class hierarchy, not from knowing the object.
  - Lesson: whether a virtual call stays indirect depends on exact flags and visibility. Check the instructions, not the slogan.
  - Holds for / re-check: the `sed -i ''` above is BSD/macOS `sed`; GNU `sed` takes `sed -i` with no empty argument. Apple clang 21; GCC has its own devirtualization passes and flags. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), GCC 13 with `-O2 -flto` devirtualized `sum_iface` speculatively: it compares the vtable slot with `Square::area`, runs the inlined body on a match and `blr` otherwise (`-fopt-info` reports it). `-fwhole-program-vtables` is only safe if no other module (for example a plugin loaded at run time) adds implementations of the class. Re-check by rebuilding and grepping for `blr` (arm64) or `call *` (x86) on your compiler.
</details>

---

## Close

Next: part 6, `lesson-06-practice-bank-06-build-config.md` (about 26 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
