/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2019 GrangerHub

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, see <https://www.gnu.org/licenses/>

===========================================================================
*/

#define ACTIVATION_ENT_RANGE            64 // default maximum range a client can activate an activation entity

// health and damage are specified in this file as subunits of
// health and damage points for better finetuning.
#define HEALTH_POINT_SUBUNIT_FACTOR     1000 // health and damage points are made up of this many sub-units.

/*
 * ALIEN weapons
 *
 * _REPEAT  - time in msec until the weapon can be used again
 * _DMG     - amount of damage the weapon does
 *
 * ALIEN_WDMG_MODIFIER - overall damage modifier for coarse tuning
 *
 */

#define ALIEN_WDMG_MODIFIER         1.0f
#define AWDM(d)                     ((int)((float)d*ALIEN_WDMG_MODIFIER))

#define ABUILDER_BUILD_REPEAT       500
#define ABUILDER_CLAW_SUBCHECKS     0
#define ABUILDER_CLAW_DMG           AWDM(20000)
#define ABUILDER_CLAW_RANGE         64.0f
#define ABUILDER_CLAW_WIDTH         4.0f
#define ABUILDER_CLAW_REPEAT        1000
#define ABUILDER_CLAW_K_SCALE       1.0f
#define ABUILDER_BLOB_DMG           AWDM(4000)
#define ABUILDER_BLOB_REPEAT        1000
#define ABUILDER_BLOB_SPEED         800.0f
#define ABUILDER_BLOB_SPEED_MOD     0.5f
#define ABUILDER_BLOB_TIME          2000

#define LEVEL0_BITE_SUBCHECKS       5
#define LEVEL0_BITE_DMG             AWDM(48000)
#define LEVEL0_BITE_RANGE           64.0f
#define LEVEL0_BITE_WIDTH           6.0f
#define LEVEL0_BITE_REPEAT          500
#define LEVEL0_BITE_K_SCALE         1.0f

#define LEVEL1_CLAW_SUBCHECKS       0
#define LEVEL1_CLAW_DMG             AWDM(32000)
#define LEVEL1_CLAW_RANGE           96.0f
#define LEVEL1_CLAW_U_RANGE         LEVEL1_CLAW_RANGE
#define LEVEL1_CLAW_WIDTH           10.0f
#define LEVEL1_CLAW_REPEAT          600
#define LEVEL1_CLAW_U_REPEAT        500
#define LEVEL1_CLAW_K_SCALE         1.0f
#define LEVEL1_CLAW_U_K_SCALE       1.0f
#define LEVEL1_GRAB_RANGE           64.0f
#define LEVEL1_GRAB_U_RANGE         LEVEL1_GRAB_RANGE
#define LEVEL1_GRAB_TIME            300
#define LEVEL1_GRAB_U_TIME          450
#define LEVEL1_PCLOUD_DMG           AWDM(4000)
#define LEVEL1_PCLOUD_RANGE         200.0f
#define LEVEL1_PCLOUD_REPEAT        2000
#define LEVEL1_PCLOUD_TIME          10000
#define LEVEL1_REGEN_MOD            1.5f
#define LEVEL1_UPG_REGEN_MOD        2.0f
#define LEVEL1_REGEN_SCOREINC       AVM(100) // score added for healing per 10s
#define LEVEL1_UPG_REGEN_SCOREINC   AVM(200)
#define LEVEL1_JUMP_MAGNITUDE       270.0f
#define LEVEL1_UPG_JUMP_MAGNITUDE   366.0f

#define LEVEL2_CLAW_SUBCHECKS       0
#define LEVEL2_CLAW_DMG             AWDM(40000)
#define LEVEL2_CLAW_RANGE           96.0f
#define LEVEL2_CLAW_U_RANGE         LEVEL2_CLAW_RANGE
#define LEVEL2_CLAW_WIDTH           12.0f
#define LEVEL2_CLAW_REPEAT          500
#define LEVEL2_CLAW_K_SCALE         1.0f
#define LEVEL2_CLAW_U_REPEAT        400
#define LEVEL2_CLAW_U_K_SCALE       1.0f
#define LEVEL2_AREAZAP_SUBCHECKS    5
#define LEVEL2_AREAZAP_DMG          AWDM(60000)
#define LEVEL2_AREAZAP_RANGE        200.0f
#define LEVEL2_AREAZAP_CHAIN_RANGE  150.0f
#define LEVEL2_AREAZAP_CHAIN_FALLOFF 8.0f
#define LEVEL2_AREAZAP_WIDTH        15.0f
#define LEVEL2_AREAZAP_REPEAT       1500
#define LEVEL2_AREAZAP_TIME         1000
#define LEVEL2_AREAZAP_MAX_TARGETS  5
#define LEVEL2_WALLJUMP_MAXSPEED    1000.0f

#define LEVEL3_CLAW_SUBCHECKS       0
#define LEVEL3_CLAW_DMG             AWDM(80000)
#define LEVEL3_CLAW_RANGE           96.0f
#define LEVEL3_CLAW_UPG_RANGE       LEVEL3_CLAW_RANGE // + 3.0f          
#define LEVEL3_CLAW_WIDTH           16.0f
#define LEVEL3_CLAW_REPEAT          700
#define LEVEL3_CLAW_K_SCALE         0.1f
#define LEVEL3_CLAW_U_REPEAT        600
#define LEVEL3_CLAW_U_K_SCALE       0.1f
#define LEVEL3_POUNCE_SUBCHECKS     0
#define LEVEL3_POUNCE_DMG           AWDM(100000)
#define LEVEL3_POUNCE_RANGE         72.0f
#define LEVEL3_POUNCE_UPG_RANGE     LEVEL3_POUNCE_RANGE + 3.0f
#define LEVEL3_POUNCE_WIDTH         16.0f
#define LEVEL3_POUNCE_TIME          700      // msec for full Dragoon pounce
#define LEVEL3_POUNCE_TIME_UPG      700      // msec for full Adv. Dragoon pounce
#define LEVEL3_POUNCE_TIME_MIN      200      // msec before which pounce cancels  
#define LEVEL3_POUNCE_REPEAT        400      // msec before a new pounce starts
#define LEVEL3_POUNCE_SPEED_MOD     0.75f    // walking speed modifier for pounce charging
#define LEVEL3_POUNCE_JUMP_MAG      700      // Dragoon pounce jump power
#define LEVEL3_POUNCE_JUMP_MAG_UPG  800      // Adv. Dragoon pounce jump power
#define LEVEL3_BOUNCEBALL_DMG       AWDM(110000)
#define LEVEL3_BOUNCEBALL_REPEAT    1000
#define LEVEL3_BOUNCEBALL_SPEED     1000.0f
#define LEVEL3_BOUNCEBALL_RADIUS    0
#define LEVEL3_BOUNCEBALL_REGEN     12500    // msec until new barb

#define LEVEL4_CLAW_SUBCHECKS       0
#define LEVEL4_CLAW_DMG             AWDM(100000)
#define LEVEL4_CLAW_RANGE           100.0f
#define LEVEL4_CLAW_WIDTH           20.0f
#define LEVEL4_CLAW_HEIGHT          22.0f
#define LEVEL4_CLAW_OFFSET          (-16.5f)
#define LEVEL4_CLAW_REPEAT          750
#define LEVEL4_CLAW_K_SCALE         1.0f

#define LEVEL4_TRAMPLE_DMG             AWDM(111000)
#define LEVEL4_TRAMPLE_SPEED           2.0f
#define LEVEL4_TRAMPLE_CHARGE_MIN      750   // minimum msec to start a charge
#define LEVEL4_TRAMPLE_CHARGE_MAX      1500  // msec to maximum charge stored
#define LEVEL4_TRAMPLE_CHARGE_TRIGGER  3000  // msec charge starts on its own
#define LEVEL4_TRAMPLE_DURATION        3000  // msec trample lasts on full charge
#define LEVEL4_TRAMPLE_STOP_PENALTY    1     // charge lost per msec when stopped
#define LEVEL4_TRAMPLE_REPEAT          1000   // msec before a trample will rehit a player

#define LEVEL4_CRUSH_DAMAGE_PER_V      500.0f // damage per falling velocity
#define LEVEL4_CRUSH_DAMAGE            120000 // to players only
#define LEVEL4_CRUSH_REPEAT            500    // player damage repeat

/*
 * ALIEN classes
 *
 * _SPEED   - fraction of Q3A run speed the class can move
 * _REGEN   - health per second regained
 *
 * ALIEN_HLTH_MODIFIER - overall health modifier for coarse tuning
 *
 */

#define ALIEN_HLTH_MODIFIER         1.0f
#define AHM(h)                      ((int)((float)h*ALIEN_HLTH_MODIFIER))

#define ALIEN_VALUE_MODIFIER        1.0f
#define AVM(h)                      ((int)((float)h*ALIEN_VALUE_MODIFIER))

#define ABUILDER_SPEED              0.9f
#define ABUILDER_VALUE              AVM(240)
#define ABUILDER_HEALTH             AHM(50000)
#define ABUILDER_REGEN              (0.04f * ABUILDER_HEALTH)
#define ABUILDER_COST               0

#define ABUILDER_UPG_SPEED          0.9f
#define ABUILDER_UPG_VALUE          AVM(300)
#define ABUILDER_UPG_HEALTH         AHM(75000)
#define ABUILDER_UPG_REGEN          (0.04f * ABUILDER_UPG_HEALTH)
#define ABUILDER_UPG_COST           0

#define LEVEL0_SPEED                1.3f
#define LEVEL0_VALUE                AVM(175)
#define LEVEL0_HEALTH               AHM(25000)
#define LEVEL0_REGEN                (0.04f * LEVEL0_HEALTH)
#define LEVEL0_COST                 0

#define LEVEL1_SPEED                1.25f
#define LEVEL1_VALUE                AVM(270)
#define LEVEL1_HEALTH               AHM(75000)
#define LEVEL1_REGEN                2000
#define LEVEL1_COST                 1

#define LEVEL1_UPG_SPEED            1.25f
#define LEVEL1_UPG_VALUE            AVM(330)
#define LEVEL1_UPG_HEALTH           AHM(100000)
#define LEVEL1_UPG_REGEN            3000
#define LEVEL1_UPG_COST             1

#define LEVEL2_SPEED                1.35f
#define LEVEL2_VALUE                AVM(420)
#define LEVEL2_HEALTH               AHM(150000)
#define LEVEL2_REGEN                4000
#define LEVEL2_COST                 1

#define LEVEL2_UPG_SPEED            1.35f
#define LEVEL2_UPG_VALUE            AVM(540)
#define LEVEL2_UPG_HEALTH           AHM(175000)
#define LEVEL2_UPG_REGEN            5000
#define LEVEL2_UPG_COST             1

#define LEVEL3_SPEED                1.1f
#define LEVEL3_VALUE                AVM(600)
#define LEVEL3_HEALTH               AHM(200000)
#define LEVEL3_REGEN                (0.03f * LEVEL3_HEALTH)
#define LEVEL3_COST                 1

#define LEVEL3_UPG_SPEED            1.1f
#define LEVEL3_UPG_VALUE            AVM(720)
#define LEVEL3_UPG_HEALTH           AHM(250000)
#define LEVEL3_UPG_REGEN            7000
#define LEVEL3_UPG_COST             1

#define LEVEL4_SPEED                1.2f
#define LEVEL4_VALUE                AVM(960)
#define LEVEL4_HEALTH               AHM(400000)
#define LEVEL4_REGEN                7000
#define LEVEL4_COST                 2

/*
 * ALIEN buildables
 *
 * _BP            - build points required for this buildable
 * _BT            - build time required for this buildable
 * _REGEN         - the amount of health per second regained
 * _SPLASHDAMGE   - the amount of damage caused by this buildable when melting
 * _SPLASHRADIUS  - the radius around which it does this damage
 *
 * CREEP_BASESIZE - the maximum distance a buildable can be from an egg/overmind
 * ALIEN_BHLTH_MODIFIER - overall health modifier for coarse tuning
 * ALIEN_BDMG_MODIFIER  - overall damage modifier for coarse tuning
 *
 */

#define ALIEN_BHLTH_MODIFIER        1.0f
#define ABHM(h)                     ((int)((float)h*ALIEN_BHLTH_MODIFIER))
#define ALIEN_BDMG_MODIFIER         1.0f
#define ABDM(d)                     ((int)((float)d*ALIEN_BDMG_MODIFIER))

#define CREEP_BASESIZE              700
#define CREEP_TIMEOUT               1000
#define CREEP_MODIFIER              0.5f
#define CREEP_ARMOUR_MODIFIER       0.75f
#define CREEP_SCALEDOWN_TIME        3000

#define PCLOUD_MODIFIER             0.5f
#define PCLOUD_ARMOUR_MODIFIER      0.75f

#define ASPAWN_BP                   10
#define ASPAWN_BT                   15000
#define ASPAWN_HEALTH               ABHM(250000)
#define ASPAWN_REGEN                8000
#define ASPAWN_SPLASHDAMAGE         ABDM(50000)
#define ASPAWN_SPLASHRADIUS         100
#define ASPAWN_CREEPSIZE            120
#define ASPAWN_VALUE                150

#define BARRICADE_BP                8
#define BARRICADE_BT                20000
#define BARRICADE_HEALTH            ABHM(300000)
#define BARRICADE_REGEN             14000
#define BARRICADE_SPLASHDAMAGE      ABDM(50000)
#define BARRICADE_SPLASHRADIUS      100
#define BARRICADE_CREEPSIZE         120
#define BARRICADE_SHRINKPROP        0.25f
#define BARRICADE_SHRINKTIMEOUT     500
#define BARRICADE_VALUE             0

#define BOOSTER_BP                  12
#define BOOSTER_BT                  15000
#define BOOSTER_HEALTH              ABHM(150000)
#define BOOSTER_REGEN               8000
#define BOOSTER_SPLASHDAMAGE        ABDM(50000)
#define BOOSTER_SPLASHRADIUS        100
#define BOOSTER_CREEPSIZE           120
#define BOOSTER_REGEN_MOD           2.0f
#define BOOSTER_VALUE               0
#define BOOST_TIME                  30000
#define BOOST_WARN_TIME             15000

#define ACIDTUBE_BP                 8
#define ACIDTUBE_BT                 15000
#define ACIDTUBE_HEALTH             ABHM(125000)
#define ACIDTUBE_REGEN              10000
#define ACIDTUBE_SPLASHDAMAGE       ABDM(50000)
#define ACIDTUBE_SPLASHRADIUS       300
#define ACIDTUBE_CREEPSIZE          120
#define ACIDTUBE_DAMAGE             ABDM(6000)
#define ACIDTUBE_RANGE              300.0f
#define ACIDTUBE_REPEAT             200
#define ACIDTUBE_REPEAT_ANIM        2000
#define ACIDTUBE_VALUE              0

#define HIVE_BP                     12
#define HIVE_BT                     20000
#define HIVE_HEALTH                 ABHM(125000)
#define HIVE_REGEN                  10000
#define HIVE_SPLASHDAMAGE           ABDM(30000)
#define HIVE_SPLASHRADIUS           200
#define HIVE_CREEPSIZE              120
#define HIVE_SENSE_RANGE            500.0f
#define HIVE_LIFETIME               3000
#define HIVE_REPEAT                 3000
#define HIVE_K_SCALE                1.0f
#define HIVE_DMG                    ABDM(80000)
#define HIVE_SPEED                  320.0f
#define HIVE_DIR_CHANGE_PERIOD      500
#define HIVE_VALUE                  0

#define HOVEL_BP                    0
#define HOVEL_BT                    15000
#define HOVEL_HEALTH                ABHM(375000)
#define HOVEL_REGEN                 20000
#define HOVEL_SPLASHDAMAGE          ABDM(20000)
#define HOVEL_SPLASHRADIUS          200
#define HOVEL_CREEPSIZE             120
#define HOVEL_VALUE                 0

#define TRAPPER_BP                  8
#define TRAPPER_BT                  12000
#define TRAPPER_HEALTH              ABHM(50000)
#define TRAPPER_REGEN               6000
#define TRAPPER_SPLASHDAMAGE        ABDM(15000)
#define TRAPPER_SPLASHRADIUS        100
#define TRAPPER_CREEPSIZE           30
#define TRAPPER_RANGE               400
#define TRAPPER_REPEAT              1000
#define TRAPPER_VALUE               0
#define LOCKBLOB_SPEED              650.0f
#define LOCKBLOB_LOCKTIME           5000
#define LOCKBLOB_DOT                0.85f // max angle = acos( LOCKBLOB_DOT )
#define LOCKBLOB_K_SCALE            1.0f

#define OVERMIND_BP                 0
#define OVERMIND_BT                 30000
#define OVERMIND_HEALTH             ABHM(750000)
#define OVERMIND_REGEN              6000
#define OVERMIND_SPLASHDAMAGE       ABDM(15000)
#define OVERMIND_SPLASHRADIUS       300
#define OVERMIND_CREEPSIZE          120
#define OVERMIND_ATTACK_RANGE       150.0f
#define OVERMIND_ATTACK_REPEAT      1000
#define OVERMIND_VALUE              300

/*
 * ALIEN misc
 *
 * ALIENSENSE_RANGE - the distance alien sense is useful for
 *
 */

#define ALIENSENSE_RANGE            1000.0f
#define REGEN_BOOST_RANGE           200.0f

#define ALIEN_POISON_TIME           5000
#define ALIEN_POISON_DMG            5000
#define ALIEN_POISON_DIVIDER        (1.0f/1.32f) //about 1.0/(time`th root of damage)

#define ALIEN_SPAWN_REPEAT_TIME     500
#define ALIEN_SPAWN_PROTECTION_TIME 3000

#define ALIEN_REGEN_DAMAGE_TIME     2000 //msec since damage that regen starts again
#define ALIEN_REGEN_NOCREEP_MOD     (1.0f) //regen off creep

#define ALIEN_MAX_FRAGS             9
#define ALIEN_MAX_CREDITS           (ALIEN_MAX_FRAGS*ALIEN_CREDITS_PER_KILL)
#define ALIEN_CREDITS_PER_KILL      400
#define ALIEN_TK_SUICIDE_PENALTY    350

/*
 * HUMAN weapons
 *
 * _REPEAT  - time between firings
 * _RELOAD  - time needed to reload
 * _PRICE   - amount in credits weapon costs
 *
 * HUMAN_WDMG_MODIFIER - overall damage modifier for coarse tuning
 *
 */

#define HUMAN_WDMG_MODIFIER         1.0f
#define HWDM(d)                     ((int)((float)d*HUMAN_WDMG_MODIFIER))

#define BLASTER_REPEAT              600
#define BLASTER_K_SCALE             1.0f
#define BLASTER_SPREAD              200
#define BLASTER_SPEED               1400
#define BLASTER_DMG                 HWDM(10000)
#define BLASTER_SIZE                5

#define RIFLE_CLIPSIZE              30
#define RIFLE_MAXCLIPS              6
#define RIFLE_REPEAT                90
#define RIFLE_K_SCALE               1.0f
#define RIFLE_RELOAD                2000
#define RIFLE_PRICE                 0
#define RIFLE_SPREAD                180
#define RIFLE_DMG                   HWDM(5000)

#define PAINSAW_PRICE               100
#define PAINSAW_REPEAT              75
#define PAINSAW_K_SCALE             1.0f
#define PAINSAW_DAMAGE              HWDM(15000)
#define PAINSAW_RANGE               40.0f
#define PAINSAW_WIDTH               0.0f
#define PAINSAW_HEIGHT              8.0f

#define GRENADE_PRICE               200
#define GRENADE_REPEAT              0
#define GRENADE_K_SCALE             1.0f
#define GRENADE_DAMAGE              HWDM(310000)
#define GRENADE_RANGE               192.0f
#define GRENADE_SPEED               400.0f

#define FRAGNADE_PRICE              250
#define FRAGNADE_REPEAT             0
#define FRAGNADE_K_SCALE            1.0f
#define FRAGNADE_FRAGMENTS          150
#define FRAGNADE_PITCH_LAYERS       5
#define FRAGNADE_SPREAD             45
#define FRAGNADE_RANGE              1000
#define FRAGNADE_FRAGMENT_DMG       HWDM(80000)
#define FRAGNADE_BLAST_DAMAGE       (FRAGNADE_FRAGMENT_DMG * FRAGNADE_FRAGMENTS)
#define FRAGNADE_BLAST_RANGE        0.0f
#define FRAGNADE_SPEED              400.0f

#define SHOTGUN_PRICE               150
#define SHOTGUN_SHELLS              8
#define SHOTGUN_PELLETS             8 //used to sync server and client side
#define SHOTGUN_MAXCLIPS            3
#define SHOTGUN_REPEAT              1000
#define SHOTGUN_K_SCALE             1.0f
#define SHOTGUN_RELOAD              2000
#define SHOTGUN_SPREAD              (6.5f)
#define SHOTGUN_DMG                 HWDM(7000)
#define SHOTGUN_RANGE               (8192 * 12)

#define LASGUN_PRICE                250
#define LASGUN_AMMO                 200
#define LASGUN_REPEAT               200
#define LASGUN_K_SCALE              1.0f
#define LASGUN_RELOAD               2000
#define LASGUN_DAMAGE               HWDM(9000)

#define MDRIVER_PRICE               350
#define MDRIVER_CLIPSIZE            5
#define MDRIVER_MAXCLIPS            4
#define MDRIVER_DMG                 HWDM(38000)
#define MDRIVER_REPEAT              1000
#define MDRIVER_K_SCALE             1.0f
#define MDRIVER_RELOAD              2000

#define CHAINGUN_PRICE              400
#define CHAINGUN_BULLETS            300
#define CHAINGUN_REPEAT             80
#define CHAINGUN_K_SCALE            1.0f
#define CHAINGUN_SPREAD             1000
#define CHAINGUN_DMG                HWDM(6000)

#define FLAMER_PRICE                400
#define FLAMER_GAS                  200
#define FLAMER_REPEAT               200
#define FLAMER_K_SCALE              2.0f
#define FLAMER_DMG                  HWDM(20000)
#define FLAMER_SPLASHDAMAGE         HWDM(10000)
#define FLAMER_RADIUS               50       // splash radius
#define FLAMER_START_SIZE           4        // initial missile bounding box
#define FLAMER_FULL_SIZE            40       // fully enlarged missile bounding box
#define FLAMER_EXPAND_TIME          800
#define FLAMER_LIFETIME             800
#define FLAMER_SPEED                300.0f
#define FLAMER_LAG                  0.65f    // the amount of player velocity that is added to the fireball

#define PRIFLE_PRICE                450
#define PRIFLE_CLIPS                40
#define PRIFLE_MAXCLIPS             5
#define PRIFLE_REPEAT               100
#define PRIFLE_K_SCALE              1.0f
#define PRIFLE_RELOAD               2000
#define PRIFLE_DMG                  HWDM(9000)
#define PRIFLE_SPEED                1200
#define PRIFLE_SIZE                 5

#define LCANNON_PRICE               600
#define LCANNON_AMMO                90
#define LCANNON_K_SCALE             1.0f
#define LCANNON_REPEAT              500
#define LCANNON_RELOAD              0
#define LCANNON_DAMAGE              HWDM(265000)
#define LCANNON_RADIUS              150      // primary splash damage radius
#define LCANNON_SIZE                5        // missile bounding box radius
#define LCANNON_SECONDARY_DAMAGE    HWDM( LCANNON_DAMAGE / LCANNON_CHARGE_AMMO )
#define LCANNON_SECONDARY_RADIUS    75       // secondary splash damage radius
#define LCANNON_SECONDARY_SPEED     350
#define LCANNON_SECONDARY_RELOAD    2000
#define LCANNON_SECONDARY_REPEAT    500
#define LCANNON_SPEED_MIN           350
#define LCANNON_CHARGE_TIME_MAX     2000
#define LCANNON_CHARGE_TIME_MIN     400
#define LCANNON_CHARGE_TIME_WARN    ( LCANNON_CHARGE_TIME_MAX - ( 2000 / 3 ) )
#define LCANNON_CHARGE_AMMO         10       // ammo cost of a full charge shot
#define LCANNON_CHARGE_AMMO_REDUCE  0.0f     // rate at which luci ammo is reduced when using +attack2
#define LCANNON_SPLASH_LIGHTARMOUR  0.50f    // reduction of damage fraction for luci splash on light armour
#define LCANNON_SPLASH_HELMET       0.75f    // reduction of damage fraction for luci splash on helmets
#define LCANNON_SPLASH_BATTLESUIT   0.50f    // reduction of damage fraction for luci splash on battlesuits

#define PORTALGUN_PRICE             700
#define PORTALGUN_AMMO              40
#define PORTALGUN_MAXCLIPS          0
#define PORTALGUN_ROUND_PRICE       20
#define PORTALGUN_REPEAT            200     // repeat period if a portal isn't created
#define PORTAL_CREATED_REPEAT       1500    // repeat period if a portal is created
#define PORTALGUN_SPEED             8000
#define PORTALGUN_SIZE              25       // missile bounding box radius
#define PORTAL_LIFETIME             120000   // max time a portal can exist
#define PORTAL_HEALTH               500000

#define LAUNCHER_PRICE              1600
#define LAUNCHER_AMMO               6
#define LAUNCHER_MAXCLIPS           0
#define LAUNCHER_ROUND_PRICE        GRENADE_PRICE
#define LAUNCHER_REPEAT             2000
#define LAUNCHER_K_SCALE            1.0f
#define LAUNCHER_RELOAD             2000
#define LAUNCHER_DAMAGE             GRENADE_DAMAGE
#define LAUNCHER_RADIUS             GRENADE_RANGE
#define LAUNCHER_SPEED              1200

#define LIGHTNING_PRICE                1000
#define LIGHTNING_AMMO                 660
#define LIGHTNING_MAXCLIPS             1
#define LIGHTNING_RELOAD               750
#define LIGHTNING_K_SCALE              0.0f
#define LIGHTNING_BOLT_DPS             HWDM(40000)
#define LIGHTNING_BOLT_CHARGE_TIME_MIN 150
#define LIGHTNING_BOLT_CHARGE_TIME_MAX 1500
#define LIGHTNING_BOLT_BEAM_DURATION   350
#define LIGHTNING_BOLT_RANGE_MAX       768
#define LIGHTNING_BOLT_NOGRND_DMG_MOD  0.50f
#define LIGHTNING_BALL_AMMO_USAGE      6
#define LIGHTNING_BALL_REPEAT          200
#define LIGHTNING_BALL_BURST_ROUNDS    6
#define LIGHTNING_BALL_BURST_DELAY     1800
#define LIGHTNING_BALL_LIFETIME        5000
#define LIGHTNING_BALL_DAMAGE          HWDM( 40000 )
#define LIGHTNING_BALL_SPLASH_DMG      HWDM( 40000 )
#define LIGHTNING_BALL_RADIUS1         150
#define LIGHTNING_BALL_RADIUS2         200
#define LIGHTNING_BALL_SIZE            5
#define LIGHTNING_BALL_SPEED           750
#define LIGHTNING_EMP_DAMAGE           HWDM( 0 )
#define LIGHTNING_EMP_SPLASH_DMG       HWDM( 0 )
#define LIGHTNING_EMP_RADIUS           0
#define LIGHTNING_EMP_SPEED            1500

#define HBUILD_PRICE                0
#define HBUILD_REPEAT               1000
#define HBUILD_HEALRATE             18000

/*
 * HUMAN upgrades
 */

#define LIGHTARMOUR_PRICE           70
#define LIGHTARMOUR_POISON_PROTECTION 1000
#define LIGHTARMOUR_PCLOUD_PROTECTION 1000000

#define HELMET_PRICE                90
#define HELMET_RANGE                1000.0f
#define HELMET_POISON_PROTECTION    1000
#define HELMET_PCLOUD_PROTECTION    1000000

#define MEDKIT_PRICE                0

#define BATTPACK_PRICE              100
#define BATTPACK_MODIFIER           1.5f //modifier for extra energy storage available

#define JETPACK_FULL_FUEL_PRICE        90
#define JETPACK_PRICE                  120
#define JETPACK_FLOAT_SPEED            136.0f //up movement speed
#define JETPACK_SINK_SPEED             184.0f //down movement speed
#define JETPACK_DISABLE_TIME           1000 //time to disable the jetpack when player damaged
#define JETPACK_DISABLE_CHANCE         0.3f
#define JETPACK_FUEL_FULL              6000 //can't exceed 32767
#define JETPACK_FUEL_LOW               1000
#define JETPACK_FUEL_USAGE             12 //every 100ms
#define JETPACK_FUEL_MIN_START         300 // Minimum fuel required to start the jet
#define JETPACK_ACT_BOOST_FUEL_USE     ( JETPACK_FUEL_USAGE * 2 )
#define JETPACK_ACT_BOOST_TIME         750
#define JETPACK_ACT_BOOST_SPEED        184.0f
#define JETPACK_FUEL_RECHARGE          5 // every 100ms
#define JETPACK_DEACTIVATION_FALL_TIME 750 // amount of time gravity is reduced after deactivation

#define BSUIT_PRICE                 400
#define BSUIT_POISON_PROTECTION     3000
#define BSUIT_PCLOUD_PROTECTION     3000000

#define MEDKIT_POISON_IMMUNITY_TIME 0
#define MEDKIT_STARTUP_TIME         4000
#define MEDKIT_STARTUP_SPEED        5

/*
 * HUMAN buildables
 *
 * _BP            - build points required for this buildable
 * _BT            - build time required for this buildable
 * _SPLASHDAMGE   - the amount of damage caused by this buildable when it blows up
 * _SPLASHRADIUS  - the radius around which it does this damage
 *
 * REACTOR_BASESIZE - the maximum distance a buildable can be from a reactor
 * REPEATER_BASESIZE - the maximum distance a buildable can be from a repeater
 * HUMAN_BHLTH_MODIFIER - overall health modifier for coarse tuning
 * HUMAN_BDMG_MODIFIER  - overall damage modifier for coarse tuning
 *
 */

#define HUMAN_BHLTH_MODIFIER        1.0f
#define HBHM(h)                     ((int)((float)h*HUMAN_BHLTH_MODIFIER))
#define HUMAN_BDMG_MODIFIER         1.0f
#define HBDM(d)                     ((int)((float)d*ALIEN_WDMG_MODIFIER))

#define REACTOR_BASESIZE            1000
#define REPEATER_BASESIZE           500
#define HUMAN_DETONATION_DELAY      5000

#define HSPAWN_BP                   10
#define HSPAWN_BT                   10000
#define HSPAWN_HEALTH               HBHM(310000)
#define HSPAWN_SPLASHDAMAGE         HBDM(50000)
#define HSPAWN_SPLASHRADIUS         100
#define HSPAWN_VALUE                ALIEN_CREDITS_PER_KILL

#define MEDISTAT_BP                 8
#define MEDISTAT_BT                 10000
#define MEDISTAT_HEALTH             HBHM(190000)
#define MEDISTAT_REPEAT             100
#define MEDISTAT_SPLASHDAMAGE       HBDM(50000)
#define MEDISTAT_SPLASHRADIUS       100
#define MEDISTAT_VALUE              0

#define MGTURRET_BP                 8
#define MGTURRET_BT                 10000
#define MGTURRET_HEALTH             HBHM(190000)
#define MGTURRET_SPLASHDAMAGE       HBDM(100000)
#define MGTURRET_SPLASHRADIUS       100
#define MGTURRET_ANGULARSPEED       8
#define MGTURRET_ACCURACY_TO_FIRE   0
#define MGTURRET_VERTICALCAP        45  // +/- maximum pitch
#define MGTURRET_REPEAT             150
#define MGTURRET_K_SCALE            1.0f
#define MGTURRET_RANGE              300.0f
#define MGTURRET_SPREAD             200
#define MGTURRET_DMG                HBDM(4000)
#define MGTURRET_SPINUP_TIME        0 // time between target sighted and fire
#define MGTURRET_VALUE              0
#define MGTURRET_DCC_ANGULARSPEED   10
#define MGTURRET_DCC_SPINUP_TIME    0
#define MGTURRET_GRAB_ANGULARSPEED  3

#define TESLAGEN_BP                 10
#define TESLAGEN_BT                 15000
#define TESLAGEN_HEALTH             HBHM(220000)
#define TESLAGEN_SPLASHDAMAGE       HBDM(50000)
#define TESLAGEN_SPLASHRADIUS       100
#define TESLAGEN_REPEAT             250
#define TESLAGEN_K_SCALE            4.0f
#define TESLAGEN_RANGE              250
#define TESLAGEN_DMG                HBDM(9000)
#define TESLAGEN_VALUE              0

#define DC_BP                       8
#define DC_BT                       10000
#define DC_HEALTH                   HBHM(190000)
#define DC_SPLASHDAMAGE             HBDM(50000)
#define DC_SPLASHRADIUS             100
#define DC_ATTACK_PERIOD            10000 // how often to spam "under attack"
#define DC_HEALRATE                 2000
#define DC_RANGE                    10000
#define DC_VALUE                    0

#define ARMOURY_BP                  10
#define ARMOURY_BT                  10000
#define ARMOURY_HEALTH              HBHM(420000)
#define ARMOURY_SPLASHDAMAGE        HBDM(50000)
#define ARMOURY_SPLASHRADIUS        100
#define ARMOURY_VALUE               0

#define REACTOR_BP                  0
#define REACTOR_BT                  20000
#define REACTOR_HEALTH              HBHM(930000)
#define REACTOR_SPLASHDAMAGE        HBDM(200000)
#define REACTOR_SPLASHRADIUS        300
#define REACTOR_ATTACK_RANGE        100.0f
#define REACTOR_ATTACK_REPEAT       1000
#define REACTOR_ATTACK_DAMAGE       HBDM(40000)
#define REACTOR_ATTACK_DCC_REPEAT   1000
#define REACTOR_ATTACK_DCC_RANGE    150.0f
#define REACTOR_ATTACK_DCC_DAMAGE   HBDM(40000)
#define REACTOR_VALUE               ( 2 * ALIEN_CREDITS_PER_KILL )

#define REPEATER_BP                 0
#define REPEATER_BT                 10000
#define REPEATER_HEALTH             HBHM(250000)
#define REPEATER_SPLASHDAMAGE       HBDM(50000)
#define REPEATER_SPLASHRADIUS       100
#define REPEATER_INACTIVE_TIME      90000
#define REPEATER_VALUE              0

/*
 * HUMAN misc
 */

#define HUMAN_SPRINT_MODIFIER       1.2f
#define HUMAN_JOG_MODIFIER          1.0f
#define HUMAN_BACK_MODIFIER         0.8f
#define HUMAN_SIDE_MODIFIER         0.9f
#define HUMAN_DODGE_SIDE_MODIFIER   2.9f
#define HUMAN_DODGE_SLOWED_MODIFIER 0.9f
#define HUMAN_DODGE_UP_MODIFIER     0.5f
#define HUMAN_DODGE_TIMEOUT         500
#define HUMAN_LAND_FRICTION         3.0f

#define STAMINA_STOP_RESTORE        30
#define STAMINA_WALK_RESTORE        15
#define STAMINA_RUN_RESTORE         7
#define STAMINA_MEDISTAT_RESTORE    30 // stacked on STOP or WALK
#define STAMINA_SPRINT_TAKE         7
#define STAMINA_JUMP_TAKE           500
#define STAMINA_DODGE_TAKE          250
#define STAMINA_MAX                 1000
#define STAMINA_BREATHING_LEVEL     0
#define STAMINA_SLOW_LEVEL          -500
#define STAMINA_BLACKOUT_LEVEL      -800

#define HUMAN_SPAWN_REPEAT_TIME     500
#define HUMAN_SPAWN_PROTECTION_TIME 5000
#define HUMAN_REGEN_DAMAGE_TIME     3000 //msec since damage before dcc repairs

#define HUMAN_MAX_CREDITS           2000
#define HUMAN_TK_SUICIDE_PENALTY    150

#define HUMAN_BUILDER_SCOREINC      50       // builders receive this many points every 10 seconds
#define ALIEN_BUILDER_SCOREINC      AVM(100)  // builders receive this many points every 10 seconds

/*
 * Misc
 */

#define MIN_FALL_DISTANCE           30.0f //the fall distance at which fall damage kicks in
#define MAX_FALL_DISTANCE           120.0f //the fall distance at which maximum damage is dealt
#define AVG_FALL_DISTANCE           ((MIN_FALL_DISTANCE+MAX_FALL_DISTANCE)/2.0f)

#define TELEPORT_PROTECTION_TIME    3000 // amount of time one is protected fromtargting after teleporting

#define BUNNY_HOP_DELAY             300  // minimum miliseconds delay between bunny hops

#define CHARGE_STAMINA_MAX          2400
#define CHARGE_STAMINA_MIN          700
#define CHARGE_STAMINA_USE_RATE     (0.50f)
#define CHARGE_STAMINA_RESTORE_RATE (0.075f)

#define DEFAULT_FREEKILL_PERIOD     "120" //seconds
#define FREEKILL_ALIEN              ALIEN_CREDITS_PER_KILL
#define FREEKILL_HUMAN              LEVEL0_VALUE

#define DEFAULT_ALIEN_BUILDPOINTS   "130"
#define DEFAULT_ALIEN_QUEUE_TIME    "12000"
#define DEFAULT_ALIEN_STAGE2_THRESH "12000"
#define DEFAULT_ALIEN_STAGE3_THRESH "24000"
#define DEFAULT_ALIEN_MAX_STAGE     "2"
#define DEFAULT_HUMAN_BUILDPOINTS   "130"
#define DEFAULT_HUMAN_QUEUE_TIME    "8000"
#define DEFAULT_HUMAN_REPEATER_BUILDPOINTS "20"
#define DEFAULT_HUMAN_REPEATER_QUEUE_TIME "2000"
#define DEFAULT_HUMAN_REPEATER_MAX_ZONES "500"
#define DEFAULT_HUMAN_STAGE2_THRESH "6000"
#define DEFAULT_HUMAN_STAGE3_THRESH "12000"
#define DEFAULT_HUMAN_MAX_STAGE     "2"

#define DAMAGE_FRACTION_FOR_KILL    0.5f //how much damage players (versus structures) need to
                                         //do to increment the stage kill counters
                                         
#define MAXIMUM_BUILD_TIME          20000 // used for pie timer
