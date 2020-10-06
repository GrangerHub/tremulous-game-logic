#ifndef MENUDEF_H
#define MENUDEF_H

#define ITEM_TYPE_ANY          -1 // invalid type
#define ITEM_TYPE_NONE          0 // no specified type
#define ITEM_TYPE_TEXT          1 // simple text
#define ITEM_TYPE_BUTTON        2 // button, basically text with a border
#define ITEM_TYPE_RADIOBUTTON   3 // toggle button, may be grouped
#define ITEM_TYPE_CHECKBOX      4 // check box
#define ITEM_TYPE_EDITFIELD     5 // editable text, associated with a cvar
#define ITEM_TYPE_SAYFIELD      6 // the chat field
#define ITEM_TYPE_CYCLE         7 // cycling list
#define ITEM_TYPE_LISTBOX       8 // scrollable list
#define ITEM_TYPE_COMBOBOX      9 // drop down scrollable list
#define ITEM_TYPE_MODEL        10 // model
#define ITEM_TYPE_OWNERDRAW    11 // owner draw, has an associated ownerdraw number
#define ITEM_TYPE_NUMERICFIELD 12 // editable text, associated with a cvar
#define ITEM_TYPE_SLIDER       13 // mouse speed, volume, etc.
#define ITEM_TYPE_YESNO        14 // boolean cvar setting
#define ITEM_TYPE_MULTI        15 // multiple list setting, enumerated
#define ITEM_TYPE_BIND         16 // keyboard control configuration

#define ALIGN_LEFT   0 // left alignment
#define ALIGN_CENTER 1 // center alignment
#define ALIGN_RIGHT  2 // right alignment
#define ASPECT_NONE  3 // no aspect compensation
#define ALIGN_NONE   4

#define VALIGN_BOTTOM 0 // bottom alignment
#define VALIGN_CENTER 1 // center alignment
#define VALIGN_TOP    2 // top alignment
#define VALIGN_NONE   3

#define ITEM_TEXTSTYLE_NORMAL          0 // normal text
#define ITEM_TEXTSTYLE_BLINK           1 // fast blinking
#define ITEM_TEXTSTYLE_PULSE           2 // slow pulsing
#define ITEM_TEXTSTYLE_SHADOWED        3 // drop shadow (need a color for this)
#define ITEM_TEXTSTYLE_OUTLINED        4 // apparently unimplemented
#define ITEM_TEXTSTYLE_OUTLINESHADOWED 5 // apparently unimplemented
#define ITEM_TEXTSTYLE_SHADOWEDMORE    6 // drop shadow (need a color for this)
#define ITEM_TEXTSTYLE_NEON            7 // glow (need a color for this)

#define WINDOW_BORDER_NONE       0 // no border
#define WINDOW_BORDER_FULL       1 // full border based on border color (single pixel)
#define WINDOW_BORDER_HORZ       2 // horizontal borders only
#define WINDOW_BORDER_VERT       3 // vertical borders only
#define WINDOW_BORDER_KCGRADIENT 4 // horizontal border using the gradient bars

#define WINDOW_STYLE_EMPTY     0 // no background
#define WINDOW_STYLE_FILLED    1 // filled with background color
#define WINDOW_STYLE_GRADIENT  2 // gradient bar based on background color
#define WINDOW_STYLE_SHADER    3 // use background shader
#define WINDOW_STYLE_TEAMCOLOR 4 // team color
#define WINDOW_STYLE_CINEMATIC 5 // cinematic

#define MENU_TRUE                         1     // uh.. true
#define MENU_FALSE                        0     // and false

#define HUD_VERTICAL   0
#define HUD_HORIZONTAL 1

// list box element types
#define LISTBOX_TEXT  0
#define LISTBOX_IMAGE 1

// list feeders
#define FEEDER_SERVERS                      0 // servers
#define FEEDER_MAPS                         1 // all maps available, in graphic format
#define FEEDER_ALIENTEAM_LIST               2 // alien team members
#define FEEDER_HUMANTEAM_LIST               3 // human team members
#define FEEDER_TEAM_LIST                    4 // team members for team voting
#define FEEDER_PLAYER_LIST                  5 // players
#define FEEDER_NEWS                         6 // news
#define FEEDER_MODS                         7 // list of available mods
#define FEEDER_DEMOS                        8 // list of available demo files
#define FEEDER_SERVERSTATUS                 9 // server status
#define FEEDER_FINDPLAYER                  10 // find player

#define FEEDER_CINEMATICS                  11 // cinematics
#define FEEDER_TREMTEAMS                   12 // teams
#define FEEDER_TREMALIENCLASSES            13 // alien classes
#define FEEDER_TREMHUMANITEMS              14 // human items
#define FEEDER_TREMHUMANARMOURYBUYWEAPON   15 // human buy weapon
#define FEEDER_TREMHUMANARMOURYBUYUPGRADES 16 // human buy upgrades
#define FEEDER_TREMHUMANARMOURYSELL        17 // human sell
#define FEEDER_TREMALIENUPGRADE            18 // alien upgrade
#define FEEDER_TREMALIENBUILD              19 // alien buildables
#define FEEDER_TREMHUMANBUILD              20 // human buildables
#define FEEDER_IGNORE_LIST                 21 // ignored players

#define FEEDER_HELP_LIST                   22 // help topics
#define FEEDER_RESOLUTIONS                 23 // display resolutions
#define FEEDER_TREMVOICECMD                24 // voice commands

// display flags
#define UI_SHOW_FAVORITESERVERS           0x00000001
#define UI_SHOW_NOTFAVORITESERVERS        0x00000002

#define UI_SHOW_VOTEACTIVE                0x00000004
#define UI_SHOW_CANVOTE                   0x00000008
#define UI_SHOW_TEAMVOTEACTIVE            0x00000010
#define UI_SHOW_CANTEAMVOTE               0x00000020

#define UI_SHOW_NOTSPECTATING             0x00000040

// owner draw types
#define CG_PLAYER_HEALTH                0
#define CG_PLAYER_HEALTH_CROSS          1
#define CG_PLAYER_AMMO_VALUE            2
#define CG_PLAYER_CLIPS_VALUE           3
#define CG_PLAYER_BUILD_TIMER           4
#define CG_PLAYER_CREDITS_VALUE         5
#define CG_PLAYER_CREDITS_VALUE_NOPAD   6
#define CG_PLAYER_STAMINA               7
#define CG_PLAYER_STAMINA_1             8
#define CG_PLAYER_STAMINA_2             9
#define CG_PLAYER_STAMINA_3             10
#define CG_PLAYER_STAMINA_4             11
#define CG_PLAYER_STAMINA_BOLT          12
#define CG_PLAYER_BOOST_BOLT            13
#define CG_PLAYER_CLIPS_RING            14
#define CG_PLAYER_BUILD_TIMER_RING      15
#define CG_PLAYER_SELECT                16
#define CG_PLAYER_SELECTTEXT            17
#define CG_PLAYER_WEAPONICON            18
#define CG_PLAYER_WALLCLIMBING          19
#define CG_PLAYER_BOOSTED               20
#define CG_PLAYER_POISON_BARBS          21
#define CG_PLAYER_ALIEN_SENSE           22
#define CG_PLAYER_HUMAN_SCANNER         23
#define CG_PLAYER_LEGACY_HUMAN_SCANNER  24
#define CG_PLAYER_USABLE_BUILDABLE      25
#define CG_PLAYER_OVERHEAT_BAR_BG       26
#define CG_PLAYER_OVERHEAT_BAR          27
#define CG_PLAYER_CHARGE_BAR_BG         28
#define CG_PLAYER_CHARGE_BAR            29
#define CG_PLAYER_CROSSHAIR             30
#define CG_PLAYER_LOCATION              31
#define CG_TEAMOVERLAY                  32
#define CG_PLAYER_CREDITS_FRACTION      33

#define CG_KILLER                       34
#define CG_SPECTATORS                   35
#define CG_FOLLOW                       36
// loading screen
#define CG_LOAD_LEVELSHOT               37
#define CG_LOAD_MEDIA                   38
#define CG_LOAD_MEDIA_LABEL             39
#define CG_LOAD_BUILDABLES              40
#define CG_LOAD_BUILDABLES_LABEL        41
#define CG_LOAD_CHARMODEL               42
#define CG_LOAD_CHARMODEL_LABEL         43
#define CG_LOAD_OVERALL                 44
#define CG_LOAD_LEVELNAME               45
#define CG_LOAD_MOTD                    46
#define CG_LOAD_HOSTNAME                47

#define CG_FPS                          48
#define CG_FPS_FIXED                    49
#define CG_TIMER                        50
#define CG_TIMER_MINS                   51
#define CG_TIMER_SECS                   52
#define CG_SNAPSHOT                     53
#define CG_LAGOMETER                    54
#define CG_SPEEDOMETER                  55
#define CG_PLAYER_CROSSHAIRNAMES        56
#define CG_STAGE_REPORT_TEXT            57
#define CG_SCRIM_SETTINGS               58
#define CG_SCRIM_WIN_CONDITION          59
#define CG_SCRIM_STATUS                 60
#define CG_ALIENS_SCRIM_TEAM_STATUS     61
#define CG_HUMANS_SCRIM_TEAM_STATUS     62
#define CG_ALIENS_SCRIM_TEAM_LABEL      63
#define CG_HUMANS_SCRIM_TEAM_LABEL      64
#define CG_ALIENS_SCORE_LABEL           65
#define CG_HUMANS_SCORE_LABEL           66
#define CG_DEMO_PLAYBACK                67
#define CG_DEMO_RECORDING               68

#define CG_CONSOLE                      69
#define CG_TUTORIAL                     70
#define CG_CLOCK                        71

#define UI_NETSOURCE                    72
#define UI_NETMAPPREVIEW                73
#define UI_NETMAPCINEMATIC              74
#define UI_SERVERREFRESHDATE            75
#define UI_SERVERMOTD                   76
#define UI_GLINFO                       77
#define UI_KEYBINDSTATUS                78
#define UI_SELECTEDMAPPREVIEW           79
#define UI_SELECTEDMAPNAME              80

#define UI_TEAMINFOPANE                 81
#define UI_ACLASSINFOPANE               82
#define UI_AUPGRADEINFOPANE             83
#define UI_HITEMINFOPANE                84
#define UI_HBUYINFOPANE                 85
#define UI_HSELLINFOPANE                86
#define UI_ABUILDINFOPANE               87
#define UI_HBUILDINFOPANE               88
#define UI_HELPINFOPANE                 89

#define CG_WARMUP                       90
#define CG_WARMUP_PLAYER_READY          91
#define CG_WARMUP_ALIENS_READY          92
#define CG_WARMUP_HUMANS_READY          93
#define CG_WARMUP_HUMANS_READY_HDR      94
#define CG_WARMUP_ALIENS_READY_HDR      95

#define UI_VOICECMDINFOPANE             96

#define CG_PLAYER_EVOLVE_COOL_DOWN_BAR  97
#define CG_PLAYER_EVOLVE_COOL_DOWN_BAR_BG 98

#define CG_PLAYER_COMPASS               99

#define CG_PLAYER_EQUIP_HUD             100
#define CG_PLAYER_JETPACK_ICON          101
#define CG_PLAYER_JETPACK_FUEL          102
#define CG_PLAYER_ARMOR_SHIELD          103
#define CG_PLAYER_ARMOR                 104
#define CG_KILLFEED                     105
#define CG_TABOVERLAYSELECTIONBAR       106

#endif
