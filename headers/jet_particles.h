// runtime/jet_particles.h
//
// Sparks, glowing dots, streaks and expanding rings that live for a moment and
// fade away: the decoration for explosions, trails, magic, weather and anything
// else that should look lively. This is an opt-in module, so include
// "jet_particles.h" yourself.
//
// Particles are decoration, never gameplay state. When the spark pool or the
// ring pool is full, a new effect is quietly dropped, so an explosion can never
// stall the game or write over a particle that is still alive. Keep collisions,
// damage and every other rule in your own objects.
//
// How a cart drives it, from the first call to the last:
//   * Keep a JetParticleSystem in a static or global variable. Call
//     jet_particles_init once in _init(), choose the maximum simultaneously
//     live sparks and rings, and check its bool result. Zero disables that kind.
//   * Emit effects whenever events occur, then call jet_particles_update once
//     per _update() so motion, fading, and recycling advance by dt.
//   * Draw the scene background first, then call jet_particles_draw in _draw().
//     Particles use world coordinates and honour the active camera and clip.
//   * Use jet_particles_clear on a level restart or scene cut. It empties the
//     system but does not release its WRAM; there is no _end() cleanup.
//
// Complete explosion example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_particles.h"
//
//   enum { SPARK_CAPACITY = 640, RING_CAPACITY = 16, BURST_COUNT = 48 };
//   static JetParticleSystem effects;
//   static bool effects_ready;
//
//   void _init(void) {
//       effects_ready = jet_particles_init(
//           &effects, SPARK_CAPACITY, RING_CAPACITY
//       );
//   }
//
//   void _update(float dt) {
//       if (!effects_ready) return;
//       if (btnp(BTN_A)) {
//           const Vector2 centre = jet_make_vector2(
//               (float)JET_WIDTH / 2.0f, (float)JET_HEIGHT / 2.0f
//           );
//           JetBurstConfig burst = jet_make_burst(
//               centre, BURST_COUNT, 220.0f, jet_make_warm_ramp()
//           );
//           jet_particles_burst(&effects, &burst);
//           jet_particles_ring(&effects, centre, 64.0f, 0.0f, 15, 31);
//       }
//       jet_particles_update(&effects, dt);
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_particles_draw(&effects);
//       jet_print("PRESS A", 16, 16, 15);
//   }
//
//   void _end(void) {
//       // The particle WRAM lasts as long as the cart does; there is nothing to release.
//   }
//
// <nocodex>
//
// A first-class VISUAL-EFFECTS PARTICLE SYSTEM — an OPT-IN runtime module. Like
// jet_pool.h, jet_pathfind.h and jet_redraw.h it is deliberately NOT pulled
// in by jet64.h: a cart that wants juice includes "jet_particles.h" itself,
// and only then does the compiler's dependency resolver pull jet_particles.c
// into the build. Carts that never spawn a particle pay zero compile cost.
//
// THE CODE IT PROMOTES: a cart grew an excellent, proven effect pool — life-
// decaying debris streaks that cool along a colour ramp, and expanding
// shockwave rings with a bright core inside a soft halo. It was per-cart and
// hand-rolled (a linear scan for a free slot, one fixed warm ramp).
// This module GENERALISES it into reusable runtime hardware: configurable
// emitters, arbitrary per-emitter colour ramps, optional gravity and drag, both
// "spark" and "ring" particle kinds, and three draw styles (a cheap single
// pixel, a glowing halo+core dot for plasma, and the motion-blurred streak).
//
// THE STORAGE IT STANDS ON: every pool is a jet_pool.h JetPool — O(1) alloc,
// O(1) recycle, dense iteration over only the live particles. The duplicated
// "bool active + O(n) scan" pattern is gone. WRAM is reserved ONCE at init; no
// malloc anywhere, exactly like the rest of the console.
//
// THE CONTRACT THAT MATTERS: particles are DECORATION. When a pool is full a
// spawn is SILENTLY DROPPED — emission never stalls the game, never overwrites a
// live particle, never allocates. The cart code it promotes did exactly this;
// the contract is preserved here.
//
// Design notes (the choices that matter):
//   * Two pools, one system            → sparks and rings are different shapes,
//                                         so each gets its own JetPool. The
//                                         particle structs are private to the
//                                         .c; a cart only ever touches the
//                                         opaque JetParticleSystem.
//   * Colour over normalised life      → each particle carries its own ramp, so
//                                         debris from rival factions can fade
//                                         through different palettes from the
//                                         same system. Resolved at draw time
//                                         from life01 (1 at birth, 0 at death).
//   * Camera/clip-aware draw           → every primitive (jet_pset, jet_circ,
//                                         jet_circfill, jet_line) already
//                                         subtracts the camera and honours the
//                                         clip rect, so particles draw in world
//                                         space and clip for free.
//   * Frame-rate independent update    → velocity, drag and gravity all scale by
//                                         dt; life decays in seconds. The same
//                                         burst looks identical at any FPS.
// Complete explosion example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_particles.h"
//
//   enum { SPARK_CAPACITY = 640, RING_CAPACITY = 16, BURST_COUNT = 48 };
//   static JetParticleSystem effects;
//
//   void _init(void) {
//       jet_particles_init(&effects, SPARK_CAPACITY, RING_CAPACITY);
//   }
//
//   void _update(float dt) {
//       if (btnp(BTN_A)) {
//           JetBurstConfig burst = jet_make_burst(
//               jet_make_vector2((float)JET_WIDTH / 2.0f, (float)JET_HEIGHT / 2.0f),
//               BURST_COUNT, 220.0f, jet_make_warm_ramp()
//           );
//           jet_particles_burst(&effects, &burst);
//       }
//       jet_particles_update(&effects, dt);
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_particles_draw(&effects);
//       jet_print("PRESS A", 16, 16, 15);
//   }
//
//   void _end(void) {}
// </nocodex>

#ifndef JET_PARTICLES_H
#define JET_PARTICLES_H

#include "jet_pool.h" // JetPool: O(1) pooled particle storage (sandbox-safe)
#include "jet_math.h" // Vector2 / JET_TAU

/**
 * Constant: JET_PARTICLES_RAMP_MAX
 * Summary: The most colour stops one JetColorRamp can hold: eight.
 * Include: jet_particles.h
 * Notes: jet_make_color_ramp copies at most this many stops from the array you give it and ignores the rest. Eight is enough for a detailed hot-to-cold fade while keeping every spark in the pool small.
 * Example:
 *   uint8_t colours[JET_PARTICLES_RAMP_MAX] = { 15, 40, 37, 31, 33 };
 */
#define JET_PARTICLES_RAMP_MAX 8u

/**
 * Constant: JET_PARTICLE_STYLE_PIXEL
 * Summary: Draws each spark as a single pixel in its ramp colour.
 * Include: jet_particles.h
 * Notes: The cheapest style. Choose it for a very dense spray, where a glow or a trail on every spark would turn into noise.
 * Example:
 *   burst.style = JET_PARTICLE_STYLE_PIXEL;
 */
#define JET_PARTICLE_STYLE_PIXEL 0u

/**
 * Constant: JET_PARTICLE_STYLE_GLOW
 * Summary: Draws each spark as a bright round core with a softer halo around it.
 * Include: jet_particles.h
 * Notes: This is the style jet_make_burst chooses for you, and a good all-round choice for plasma, fire and magic. JetBurstConfig.glow_radius sets the size of the core in pixels.
 * Example:
 *   burst.style = JET_PARTICLE_STYLE_GLOW;
 */
#define JET_PARTICLE_STYLE_GLOW 1u

/**
 * Constant: JET_PARTICLE_STYLE_STREAK
 * Summary: Draws each spark as a glowing streak that trails behind it along its path.
 * Include: jet_particles.h
 * Notes: Use it for fast debris, rain, thruster exhaust and motion-blurred sparks. JetBurstConfig.streak_time sets the length of the trail: the distance the spark covers in that many seconds.
 * Example:
 *   burst.style = JET_PARTICLE_STYLE_STREAK;
 */
#define JET_PARTICLE_STYLE_STREAK 2u

// The defaults jet_make_burst fills in, so an ordinary explosion needs only a
// centre, a count, a speed and a ramp.
#define JET_PARTICLE_DEFAULT_DRAG        0.9f   // velocity bled per second (let debris fly, then settle)
#define JET_PARTICLE_DEFAULT_STREAK_TIME 0.024f // seconds of velocity drawn as a streak
#define JET_PARTICLE_DEFAULT_GLOW_RADIUS 1u     // core radius (px) for the GLOW style
#define JET_PARTICLE_DEFAULT_LIFE_MIN    0.5f   // shortest debris lifetime (s)
#define JET_PARTICLE_DEFAULT_LIFE_MAX    1.1f   // longest debris lifetime (s)

// A burst leans its speeds towards the fast end, so the blast looks like an
// expanding shell rather than a soft cloud: speeds run from
// (peak * JET_PARTICLE_RIM_BIAS) up to peak.
#define JET_PARTICLE_RIM_BIAS 0.45f

// A spark lives at least this many seconds, so its lifetime is never zero and
// working out the share of life it has left can never divide by zero.
#define JET_PARTICLE_MIN_LIFE 0.0001f

// How long a shockwave ring lives by default: the seconds it takes to grow from
// nothing to its full radius and fade out. It came from the cart code this
// module promotes.
#define JET_PARTICLE_RING_LIFE 0.42f

// For the first (1 - this) share of its life a ring shows its bright core
// colour; after that the whole ring dims to the halo colour.
#define JET_PARTICLE_RING_CORE_CUTOFF 0.45f

/**
 * Struct: JetColorRamp
 * Summary: A short run of colours that a spark fades through as it ages, hottest first. stops[0] is the colour at birth, when the spark's remaining life (life01) is 1; stops[count-1] is the colour at death, when life01 is 0. Every spark carries its own copy, so different emitters can use different ramps in one system.
 * Fields:
 *   - stops: The colours, as palette indices, hottest first. Only the first count entries mean anything.
 *   - count: How many entries of stops are in use, from 1 to JET_PARTICLES_RAMP_MAX.
 * Include: jet_particles.h
 * Notes: Build one with jet_make_color_ramp or jet_make_warm_ramp rather than filling in the fields yourself, so that count can never run past the end of the array.
 * Example:
 *   static const uint8_t plasma_colours[] = { 15, 28, 27, 1 };
 *   JetColorRamp plasma_ramp = jet_make_color_ramp(plasma_colours, 4);
 */
typedef struct {
    uint8_t stops[JET_PARTICLES_RAMP_MAX];
    uint8_t count;
} JetColorRamp;

/**
 * Function: jet_make_color_ramp
 * Summary: Builds a colour ramp from an array of palette indices, hottest first.
 * Parameters:
 *   - stops: The colours, as palette indices, ordered from hot (birth) to cold (death). May be NULL only when count is 0.
 *   - count: How many entries to copy. More than JET_PARTICLES_RAMP_MAX is cut back to that many.
 * Returns: A filled-in JetColorRamp, handed back by value. A ramp with a count of 0 always gives palette index 0.
 * Include: jet_particles.h
 * Notes: The stops are copied into the ramp, so the array you passed can go away afterwards. Give the ramp to a burst or an emit call and each spark keeps its own copy.
 * Example:
 *   static const uint8_t teal[] = { 15, 28, 27, 1 };
 *   JetColorRamp faction = jet_make_color_ramp(teal, 4);
 */
static inline JetColorRamp jet_make_color_ramp(const uint8_t* stops, uint32_t count) {
    JetColorRamp ramp;
    if (count > JET_PARTICLES_RAMP_MAX) count = JET_PARTICLES_RAMP_MAX;
    ramp.count = (uint8_t)count;
    for (uint32_t i = 0; i < JET_PARTICLES_RAMP_MAX; i++) {
        ramp.stops[i] = (stops && i < count) ? stops[i] : 0u;
    }
    return ramp;
}

/**
 * Function: jet_make_warm_ramp
 * Summary: Gives you the standard warm explosion ramp: white, then yellow, orange, red and deep red, as Aurora palette indices.
 * Parameters:
 *   - None.
 * Returns: A ready-made warm JetColorRamp, handed back by value.
 * Include: jet_particles.h
 * Notes: A good default for fire, sparks and ordinary debris. For plasma tinted in a faction's colours, make your own ramp with jet_make_color_ramp.
 * Example:
 *   JetBurstConfig b = jet_make_burst(pos, 32, 120.0f, jet_make_warm_ramp());
 */
static inline JetColorRamp jet_make_warm_ramp(void) {
    // Aurora indices: white, yellow, orange, red, deep red. A streak walks this
    // table from hot to cold as it cools.
    static const uint8_t warm[] = { 15u, 40u, 37u, 31u, 33u };
    return jet_make_color_ramp(warm, (uint32_t)(sizeof(warm) / sizeof(warm[0])));
}

/**
 * Function: jet_color_ramp_sample
 * Summary: Tells you which colour a ramp shows for a given share of remaining life.
 * Parameters:
 *   - ramp: The ramp to read. NULL gives palette index 0.
 *   - life01: The share of life still left: 1 at birth (the hottest stop), 0 at death (the coldest). A value outside 0 to 1 is held at the nearer end.
 * Returns: The palette index of the nearest stop.
 * Include: jet_particles.h
 * Notes: This is the same nearest-stop rule the drawing uses, made public so a cart can colour a matching shape (a blob's core, say) to suit its debris. The GLOW and ring styles draw their halo one stop cooler than this.
 * Example:
 *   uint8_t hot = jet_color_ramp_sample(&ramp, 1.0f);
 */
static inline uint8_t jet_color_ramp_sample(const JetColorRamp* ramp, float life01) {
    if (!ramp || ramp->count == 0u) return 0u;
    life01 = jet_clampf(life01, 0.0f, 1.0f);
    // Hot stop (index 0) at life01 = 1, cold stop (count-1) at life01 = 0.
    int last = (int)ramp->count - 1;
    int idx = (int)((1.0f - life01) * (float)last + 0.5f);
    if (idx < 0) idx = 0;
    if (idx > last) idx = last;
    return ramp->stops[idx];
}

/**
 * Struct: JetBurstConfig
 * Summary: Everything about one burst of sparks: where it starts, how many sparks to try for, how fast they fly and how long they live, how widely they scatter, how they fall and slow, and how they are drawn. Build one with jet_make_burst, then change only the fields you care about.
 * Fields:
 *   - origin: The centre the sparks fly out from, as a world position.
 *   - count: How many sparks to try to spawn. Each one is quietly dropped if the pool is full.
 *   - base_angle: The direction the spray points, in radians. Ignored when spread makes it a full circle.
 *   - spread: How far either side of base_angle a spark may fly, in radians. JET_PI or more (or 0 or less) sprays in every direction, a full 360 degrees.
 *   - speed_min: The slowest launch speed, in pixels a second.
 *   - speed_max: The fastest launch speed, in pixels a second.
 *   - life_min: The shortest life, in seconds.
 *   - life_max: The longest life, in seconds.
 *   - drag: How much of its speed a spark loses each second, as a fraction: 0 loses none, 1 is fully damped in about a second.
 *   - gravity: A steady pull added to the velocity every second, in pixels a second squared. Zero lets the sparks drift freely.
 *   - ramp: The colours the sparks fade through as they age.
 *   - style: How every spark in the burst is drawn: JET_PARTICLE_STYLE_PIXEL, JET_PARTICLE_STYLE_GLOW or JET_PARTICLE_STYLE_STREAK.
 *   - glow_radius: The size of the bright core in pixels, for the GLOW style.
 *   - streak_time: The length of the trail for the STREAK style, as the distance the spark covers in this many seconds.
 * Include: jet_particles.h
 * Notes: Pass its address to jet_particles_burst. jet_make_burst fills in sensible defaults (speeds leaning towards the fast end, warm-debris lifetimes, light drag, no gravity), so most bursts only set origin, count, the peak speed and the ramp.
 * Example:
 *   JetBurstConfig burst = jet_make_burst(hit_pos, 32, 140.0f, jet_make_warm_ramp());
 *   burst.gravity.y = 80.0f;
 */
typedef struct {
    Vector2       origin;
    uint32_t      count;
    float         base_angle;
    float         spread;
    float         speed_min;
    float         speed_max;
    float         life_min;
    float         life_max;
    float         drag;
    Vector2       gravity;
    JetColorRamp ramp;
    uint8_t       style;
    uint8_t       glow_radius;
    float         streak_time;
} JetBurstConfig;

/**
 * Function: jet_make_burst
 * Summary: Builds a JetBurstConfig for an explosion in every direction, with sensible defaults, ready for jet_particles_burst or for you to adjust first.
 * Parameters:
 *   - origin: The centre of the blast, as a world position.
 *   - count: How many sparks to try to spawn.
 *   - peak_speed: The fastest launch speed, in pixels a second. The slowest is peak_speed * JET_PARTICLE_RIM_BIAS, so the blast looks like an expanding shell.
 *   - ramp: The colours the debris fades through, for example jet_make_warm_ramp().
 * Returns: A filled-in JetBurstConfig, handed back by value: a full 360 degree spray, the default debris lifetimes, light drag, no gravity, and the GLOW style.
 * Include: jet_particles.h
 * Notes: For a spray in one direction (a muzzle flash, a thruster cone), set base_angle and a spread above 0 and below JET_PI on the returned struct before you emit. For debris that falls, set gravity.
 * Example:
 *   JetBurstConfig b = jet_make_burst(blob_pos, 40, 160.0f, jet_make_warm_ramp());
 *   jet_particles_burst(&fx, &b);
 */
static inline JetBurstConfig jet_make_burst(Vector2 origin, uint32_t count, float peak_speed, JetColorRamp ramp) {
    JetBurstConfig b;
    b.origin      = origin;
    b.count       = count;
    b.base_angle  = 0.0f;
    b.spread      = JET_PI; // full circle by default
    b.speed_min   = peak_speed * JET_PARTICLE_RIM_BIAS;
    b.speed_max   = peak_speed;
    b.life_min    = JET_PARTICLE_DEFAULT_LIFE_MIN;
    b.life_max    = JET_PARTICLE_DEFAULT_LIFE_MAX;
    b.drag        = JET_PARTICLE_DEFAULT_DRAG;
    b.gravity     = jet_make_vector2(0.0f, 0.0f);
    b.ramp        = ramp;
    b.style       = (uint8_t)JET_PARTICLE_STYLE_GLOW;
    b.glow_radius = (uint8_t)JET_PARTICLE_DEFAULT_GLOW_RADIUS;
    b.streak_time = JET_PARTICLE_DEFAULT_STREAK_TIME;
    return b;
}

/**
 * Struct: JetParticleSystem
 * Summary: One complete particle system: a pool of sparks and a pool of expanding rings, each reserved once when you call jet_particles_init. A cart keeps one (or several) and drives it with init, emit, update and draw. The particles themselves are private to jet_particles.c and are never touched directly.
 * Fields:
 *   - sparks: The pool of sparks: explosion debris, trails and glowing dots that fade as their life runs out.
 *   - rings: The pool of expanding shockwave rings.
 * Include: jet_particles.h
 * Notes: Do not look inside the fields; use the calls below. A system that is inert, because init failed or was never called, accepts emit calls and drops them, and updates and draws nothing, so it is always safe to drive.
 * Example:
 *   static JetParticleSystem effects;
 *   void _init(void) { jet_particles_init(&effects, 640, 16); }
 *   void _update(float dt) { jet_particles_update(&effects, dt); }
 *   void _draw(void) { jet_particles_draw(&effects); }
 */
typedef struct {
    JetPool sparks;
    JetPool rings;
} JetParticleSystem;

/**
 * Function: jet_make_particle_system
 * Summary: Gives you an empty, inert particle system: a safe value to store. You still have to call jet_particles_init before it can spawn anything.
 * Parameters:
 *   - None.
 * Returns: A zeroed JetParticleSystem, handed back by value, with both pools inert.
 * Include: jet_particles.h
 * Notes: An inert system quietly drops every emit and draws nothing, so a cart can hold one before _init() runs.
 * Example:
 *   JetParticleSystem fx = jet_make_particle_system();
 */
static inline JetParticleSystem jet_make_particle_system(void) {
    JetParticleSystem sys;
    sys.sparks = jet_make_pool();
    sys.rings  = jet_make_pool();
    return sys;
}

/**
 * Function: jet_particles_init
 * Summary: Reserves the spark pool and the ring pool from WRAM, the cartridge's working memory, and makes the system ready to emit.
 * Parameters:
 *   - sys: The system to set up.
 *   - spark_capacity: The most sparks (debris and trails) alive at once. Any spawn beyond that is dropped. 0 turns sparks off.
 *   - ring_capacity: The most shockwave rings alive at once. Any spawn beyond that is dropped. 0 turns rings off.
 * Returns: true if both pools you asked for were reserved; false if WRAM ran out. A pool that failed is left inert and quietly drops its spawns.
 * Include: jet_particles.h
 * Notes: Call it only from _init(). It reserves from WRAM's bump allocator, which hands memory out and never takes it back, so every extra call leaks WRAM. About 640 sparks and 16 rings suit a busy bullet-hell; if WRAM is tight, work out the cost with jet_pool_storage_bytes. If only one of the two pools could be reserved, the system still works for that pool.
 * Example:
 *   JetParticleSystem fx;
 *   jet_particles_init(&fx, 640, 16);
 */
bool jet_particles_init(JetParticleSystem* sys, uint32_t spark_capacity, uint32_t ring_capacity);

/**
 * Function: jet_particles_burst
 * Summary: Sprays a cone, or a full circle, of sparks out from one point. This is the call for explosions, plasma splatter, muzzle sparks and conversion bursts.
 * Parameters:
 *   - sys: The system to spawn into.
 *   - cfg: The burst to make (see JetBurstConfig and jet_make_burst). Must not be NULL.
 * Returns: Nothing.
 * Include: jet_particles.h
 * Notes: It spawns up to cfg->count sparks. Any that do not fit in the pool are quietly dropped, because decoration must never stall the game. The scatter angles and speeds come from the runtime's random numbers, so seed them (jet_srand for a burst you can repeat exactly, jet_randomise for one that varies). A spread of JET_PI or more sprays a full circle; a spread above 0 and below JET_PI makes a jet pointed along cfg->base_angle.
 * Example:
 *   JetBurstConfig b = jet_make_burst(pos, 48, 200.0f, jet_make_warm_ramp());
 *   jet_particles_burst(&fx, &b);
 */
void jet_particles_burst(JetParticleSystem* sys, const JetBurstConfig* cfg);

/**
 * Function: jet_particles_emit
 * Summary: Spawns one spark with a position and a velocity you choose, for fine work such as a thruster trail or a single aimed muzzle spark.
 * Parameters:
 *   - sys: The system to spawn into.
 *   - pos: Where the spark starts, as a world position.
 *   - vel: How fast it moves and in which direction, in pixels a second. This is a whole velocity, not just a speed.
 *   - life: How long it lives, in seconds. Anything below JET_PARTICLE_MIN_LIFE is raised to that.
 *   - ramp: The colours it fades through. NULL fades through palette index 0.
 *   - style: How it is drawn: JET_PARTICLE_STYLE_PIXEL, JET_PARTICLE_STYLE_GLOW or JET_PARTICLE_STYLE_STREAK.
 * Returns: Nothing.
 * Include: jet_particles.h
 * Notes: Quietly dropped when the spark pool is full. The spark gets the default drag and no gravity; for a spark that falls, or a spray in a cone, go through jet_particles_burst with a JetBurstConfig set up the way you want. jet_vector2_from_angle(heading, speed) is the easy way to build vel.
 * Example:
 *   Vector2 v = jet_vector2_from_angle(ship_angle + JET_PI, 60.0f);
 *   jet_particles_emit(&fx, exhaust_pos, v, 0.3f, &trail_ramp, JET_PARTICLE_STYLE_GLOW);
 */
void jet_particles_emit(JetParticleSystem* sys, Vector2 pos, Vector2 vel, float life,
                         const JetColorRamp* ramp, uint8_t style);

/**
 * Function: jet_particles_ring
 * Summary: Starts an expanding shockwave ring that grows out from a point to max_radius and fades, for explosions, blob splits and conversion pulses.
 * Parameters:
 *   - sys: The system to spawn into.
 *   - pos: The centre, as a world position.
 *   - max_radius: The radius, in pixels, the ring reaches at the end of its life.
 *   - life: How long it lives, in seconds. 0 or less uses JET_PARTICLE_RING_LIFE.
 *   - core: The colour of the bright inner ring while it is young, as a palette index.
 *   - glow: The colour of the soft outer halo, as a palette index. The whole ring turns this colour once it ages past the core cutoff.
 * Returns: Nothing.
 * Include: jet_particles.h
 * Notes: Quietly dropped when the ring pool is full. The radius grows at a steady rate with age, from 0 to max_radius. The ring is drawn as a halo circle with a brighter core circle just inside it, the same shockwave the asteroids cart draws.
 * Example:
 *   jet_particles_ring(&fx, blast_pos, source_radius * 2.6f, 0.0f, 15, 31);
 */
void jet_particles_ring(JetParticleSystem* sys, Vector2 pos, float max_radius, float life,
                         uint8_t core, uint8_t glow);

/**
 * Function: jet_particles_update
 * Summary: Moves every live particle on by dt: applies gravity and drag, moves each spark by its velocity, counts down each life, and hands dead particles back to their pools.
 * Parameters:
 *   - sys: The system to move on.
 *   - dt: The time since the last update, in seconds.
 * Returns: Nothing.
 * Include: jet_particles.h
 * Notes: Call it once in every _update, before jet_particles_draw. All motion and fading scale with dt, so a burst looks the same at any frame rate. A dt of zero or less does nothing. A dead particle's slot goes back to its pool at once, so the next emit can reuse it.
 * Example:
 *   jet_particles_update(&fx, dt);
 */
void jet_particles_update(JetParticleSystem* sys, float dt);

/**
 * Function: jet_particles_draw
 * Summary: Draws every live particle at its world position. Each spark's colour comes from its ramp and how much life it has left; each ring's size comes from its age.
 * Parameters:
 *   - sys: The system to draw.
 * Returns: Nothing.
 * Include: jet_particles.h
 * Notes: It draws through jet_pset, jet_circ, jet_circfill and jet_line, which take the camera's offset off every position and stay inside the clip rectangle. So set the camera with jet_cam_set for world effects exactly as you do for the rest of your scene. Drawing changes nothing in the system, so within a frame you may update and draw in either order.
 * Example:
 *   jet_particles_draw(&fx);
 */
void jet_particles_draw(const JetParticleSystem* sys);

/**
 * Function: jet_particles_clear
 * Summary: Removes every live particle at once, leaving both pools empty.
 * Parameters:
 *   - sys: The system to clear.
 * Returns: Nothing.
 * Include: jet_particles.h
 * Notes: Use it on a level restart or a scene cut, so old debris does not hang around. It is cheap, because it only resets the pools' free lists, and it gives back no WRAM.
 * Example:
 *   jet_particles_clear(&fx); // wipe effects between waves
 */
void jet_particles_clear(JetParticleSystem* sys);

/**
 * Function: jet_particles_spark_count
 * Summary: Tells you how many sparks are alive right now.
 * Parameters:
 *   - sys: The system to ask.
 * Returns: The number of live sparks, or 0 for an inert system.
 * Include: jet_particles.h
 * Notes: Handy for a debugging overlay, or for skipping a low-priority emit when the screen is already full of sparks.
 * Example:
 *   if (jet_particles_spark_count(&fx) < 600) jet_particles_burst(&fx, &b);
 */
uint32_t jet_particles_spark_count(const JetParticleSystem* sys);

/**
 * Function: jet_particles_ring_count
 * Summary: Tells you how many shockwave rings are alive right now.
 * Parameters:
 *   - sys: The system to ask.
 * Returns: The number of live rings, or 0 for an inert system.
 * Include: jet_particles.h
 * Notes: It takes the same small time however many rings there are. Useful for a debugging overlay, or for holding back a low-priority shockwave when the ring pool is nearly full.
 * Example:
 *   uint32_t rings = jet_particles_ring_count(&fx);
 */
uint32_t jet_particles_ring_count(const JetParticleSystem* sys);

#endif // JET_PARTICLES_H
