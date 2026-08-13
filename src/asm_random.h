/* ============================================================================
 * asm_random.h — faithful port of the BrickBlaster 1999 random generator.
 *
 * See asm_random.c for the full rationale.  Short version: this generator is
 * DELIBERATELY defective (unseeded, deterministic, period ~ a few thousand).
 * That is the shipped 1999 behaviour and the project owner chose behaviour
 * parity.  Do NOT "fix" it and do NOT replace calls with GetRandomValue/rand.
 * ========================================================================== */
#ifndef ASM_RANDOM_H
#define ASM_RANDOM_H

#include <stdint.h>

/* Restore the exact state the 1999 binary has when it reaches the intro:
 * .data initial values alea1..alea4 = 0 (MAIN.ASM:5094-5097) followed by the
 * boot-time init_random call (MAIN.ASM:46 → 5049-5070), which — with its CMOS
 * clock reads commented out — advances the generator exactly twice. */
void asm_random_reset(void);

/* MAIN.ASM:5074-5091 calc_random.  Advances the state and returns the new
 * alea4 (0..65535, zero-extended as the ASM leaves it in EAX).
 * The 1999 frame loop also calls this once per frame (DRAW.ASM:110,
 * wait_synchro) — stream parity requires reproducing that call. */
uint32_t asm_calc_random(void);

/* MAIN.ASM:5103-5127 get_random.  Returns 0..n INCLUSIVE.
 * Builds the smallest all-ones bit mask covering n, then draws calc_random
 * (at least once, even for n == 0) and rejects while (draw & mask) > n.
 * n is treated as unsigned 32-bit, exactly like EAX in the ASM. */
int asm_get_random(int n);

/* OPTIONAL — NOT called by default, and not called anywhere in 1999 behaviour.
 * Reproduces what init_random (MAIN.ASM:5049-5070) was *meant* to do before
 * its CMOS clock reads (out 70h / in 71h) were commented out: warm the
 * generator up by a number of steps derived from the RTC seconds register.
 * `seconds` plays the role of every `in al,71h` read (the original re-reads
 * the register at each iteration; here it is constant), giving
 * (seconds+1) * (seconds+2) warm-up steps — i.e. at most 60 distinct streams.
 * Call it after asm_random_reset() only if the owner later decides to trade
 * strict 1999 determinism for the intended (still weak) clock seeding. */
void asm_random_seed_cmos(uint8_t seconds);

#endif /* ASM_RANDOM_H */
