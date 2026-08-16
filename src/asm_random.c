/* ============================================================================
 * asm_random.c — bit-exact port of the BrickBlaster 1999 random generator.
 *
 * Source of truth: BrickBlaster/work400/Blaster/MAIN.ASM
 *   - calc_random   MAIN.ASM:5074-5091
 *   - state         MAIN.ASM:5094-5097  (alea1..alea4 dw 0)
 *   - get_random    MAIN.ASM:5103-5127
 *   - init_random   MAIN.ASM:5049-5070  (called once at boot, MAIN.ASM:46)
 *   - per-frame tick DRAW.ASM:110       (wait_synchro calls calc_random)
 *
 * ---------------------------------------------------------------------------
 * WHY THIS MODULE IS DELIBERATELY DEFECTIVE — DO NOT "FIX" IT
 * ---------------------------------------------------------------------------
 * The project owner decided on BEHAVIOUR parity with the 1999 binary, not
 * statistical quality.  The shipped generator has three genuine defects, all
 * reproduced here on purpose:
 *
 * 1. alea1 is NEVER written anywhere in the sources.  It stays 0 forever, so
 *    `add ax,71` always starts from 0 and `xor ax,alea1` is a no-op.  The
 *    effective state is only (alea2, alea3, alea4).
 *
 * 2. The seeding was supposed to come from the CMOS real-time clock, but the
 *    port reads are commented out in the shipped sources:
 *        MAIN.ASM:5053-5054   ;out 70h,al   ;in al,71h
 *        MAIN.ASM:5061-5062   ;out 70h,al   ;in al,71h
 *    With AL forced to 0 and then `inc al`, both loop counters are 1, so
 *    init_random degenerates to exactly TWO calc_random warm-up calls.  The
 *    generator is therefore fully deterministic: every 1999 game session sees
 *    the same stream.
 *
 * 3. The stream is tiny.  Measured from the all-zero state, the state
 *    sequence enters a cycle after 2603 steps and then repeats with period
 *    4811; only 6773 distinct 16-bit outputs of the 65536 possible ever occur
 *    over the whole tail+cycle (7414 draws).  Combined with get_random's
 *    mask-and-reject scheme this visibly skews every bounded draw — e.g.
 *    get_random(23) hits value 9 about 23% MORE often and value 2 about 15%
 *    LESS often than uniform (measured over 1e6 draws).
 *
 * get_random(n) itself is INCLUSIVE: it builds the smallest all-ones mask
 * covering n, draws, and rejects while (draw & mask) > n — so it returns
 * 0..n, which is n+1 values.  Every call-site in MAIN.ASM was written for
 * that convention (e.g. `mov eax,1 / call get_random` is a coin flip).
 *
 * Someone WILL be tempted to seed this from time(), widen the state, or swap
 * in a real PRNG.  Doing so changes demo-mode level selection, powerup drop
 * cadence, monster spawn columns and ball speed patterns away from the 1999
 * binary — the exact thing this module exists to preserve.  If better
 * randomness is ever wanted, it must be an explicit product decision;
 * asm_random_seed_cmos() below is the only sanctioned knob.
 *
 * The module is standalone C99, no raylib, no dependencies.
 * ========================================================================== */

#include "asm_random.h"

/* MAIN.ASM:5094-5097 — four 16-bit words, all zero-initialised in .data.
 * alea1 is kept (and kept updated — i.e. never) for fidelity of intent. */
static uint16_t alea1 = 0;
static uint16_t alea2 = 0;
static uint16_t alea3 = 0;
static uint16_t alea4 = 0;

/* --------------------------------------------------------------------------
 * calc_random — MAIN.ASM:5074-5091, transcribed with x86 carry semantics.
 * All arithmetic is 16-bit; uint32_t intermediates capture the carry flag.
 * -------------------------------------------------------------------------- */
uint32_t asm_calc_random(void)
{
    uint32_t t, cf;

    t  = (uint32_t)alea1 + 71u;             /* mov ax,alea1 ; add ax,71      */
    cf = t >> 16;
    t  = (t & 0xFFFFu) + alea3 + cf;        /* adc ax,alea3                  */
    cf = t >> 16;
    alea3 = (uint16_t)t;                    /* mov alea3,ax                  */

    /* `mov ax,alea2` does not touch flags: CF survives from the adc above. */
    t  = (uint32_t)alea2 + 32111u + cf;     /* mov ax,alea2 ; adc ax,32111   */
    cf = t >> 16;
    t  = (t & 0xFFFFu) + alea4 + cf;        /* adc ax,alea4                  */
    t &= 0xFFFFu;
    t ^= alea1;                             /* xor ax,alea1  (clears CF)     */
    alea2 = (uint16_t)t;                    /* mov alea2,ax                  */

    /* rcl alea3,1 — CF in is 0 (cleared by the xor); CF out is bit 15. */
    cf    = (uint32_t)(alea3 >> 15) & 1u;
    alea3 = (uint16_t)(alea3 << 1);

    t  = t + alea4 + cf;                    /* adc ax,alea4                  */
    cf = t >> 16;
    t &= 0xFFFFu;

    /* rcr ax,1 — CF in enters bit 15, bit 0 falls out (discarded). */
    t = (cf << 15) | (t >> 1);
    alea4 = (uint16_t)t;                    /* mov alea4,ax                  */

    /* The ASM returns with the result in AX; callers reach it through EAX,
     * whose upper half is 0 at every live call site. */
    return t;
}

/* --------------------------------------------------------------------------
 * get_random — MAIN.ASM:5103-5127.  Returns 0..n INCLUSIVE.
 *
 *   mov ecx,0                 ; mask = 0
 *   @@again: shr eax,1 / stc / rcl ecx,1  while eax != 0
 *                             ; mask = (1 << bit_length(n)) - 1
 *   @@cont:  call calc_random ; note: runs at least once, even for n == 0
 *            and eax,ecx
 *            cmp eax,ebx / ja @@cont      ; UNSIGNED reject while draw > n
 * -------------------------------------------------------------------------- */
int asm_get_random(int n)
{
    uint32_t limit = (uint32_t)n;           /* EAX/EBX are unsigned in ASM   */
    uint32_t mask  = 0;
    uint32_t v     = limit;
    uint32_t draw;

    while (v != 0) {                        /* @@again mask-building loop    */
        v >>= 1;                            /* shr eax,1                     */
        mask = (mask << 1) | 1u;            /* stc ; rcl ecx,1               */
    }

    do {                                    /* @@cont                        */
        draw = asm_calc_random() & mask;    /* call calc_random; and eax,ecx */
    } while (draw > limit);                 /* cmp eax,ebx ; ja @@cont       */

    return (int)draw;
}

/* --------------------------------------------------------------------------
 * asm_random_reset — the exact 1999 startup state.
 *
 * .data zeros (MAIN.ASM:5094-5097) + the boot call to init_random
 * (MAIN.ASM:46).  With the CMOS reads commented out, init_random is:
 *     al = 0 ; inc al ; ecx = 1
 *     @@init_random:  call calc_random          ; 1st warm-up step
 *                     push ecx
 *                     al = 0 ; inc al ; ecx = 1
 *     @@init_random2: call calc_random          ; 2nd warm-up step
 *                     loop @@init_random2       ; ecx 1→0, falls through
 *                     pop ecx
 *                     loop @@init_random        ; ecx 1→0, falls through
 * i.e. exactly two calc_random calls, deterministically.
 * -------------------------------------------------------------------------- */
void asm_random_reset(void)
{
    alea1 = 0;
    alea2 = 0;
    alea3 = 0;
    alea4 = 0;
    asm_calc_random();
    asm_calc_random();
}

/* --------------------------------------------------------------------------
 * asm_random_seed_cmos — OPTIONAL, NOT used by default.
 *
 * What init_random (MAIN.ASM:5049-5070) intended before the `out 70h/in 71h`
 * pairs were commented out: read CMOS register 0 (RTC seconds, 0..59) and use
 * it as loop counts for warm-up rounds.  `seconds` stands in for every read
 * (the original re-reads the ticking register; a constant is the faithful
 * approximation available without an RTC).  Structure mirrors the ASM:
 * outer runs (seconds+1) times, each outer step = 1 calc_random + inner
 * (seconds+1) calc_random calls.
 *
 * This yields at most 60 distinct streams — weak by design.  It exists so the
 * owner can later opt into "intended 1999" behaviour instead of "shipped
 * 1999" behaviour without rewriting the module.  Call after
 * asm_random_reset(); nothing in the port calls it.
 * -------------------------------------------------------------------------- */
void asm_random_seed_cmos(uint8_t seconds)
{
    uint32_t outer, inner;

    alea1 = 0;                              /* restart from .data zeros      */
    alea2 = 0;
    alea3 = 0;
    alea4 = 0;

    for (outer = (uint32_t)seconds + 1u; outer != 0; outer--) {
        asm_calc_random();                  /* @@init_random body            */
        for (inner = (uint32_t)seconds + 1u; inner != 0; inner--) {
            asm_calc_random();              /* @@init_random2 body           */
        }
    }
}
