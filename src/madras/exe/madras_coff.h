/*
   Copyright (C) 2004 - 2018 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)

   This file is part of MAQAO.

  MAQAO is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 3
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

//*
printf (
"     @@@       '@@'  .@#:                                   \n"
"    @@@        @@@  #@@                                     \n"
"   @@@#       `@@@  #@@+                                    \n"
"   @@@@`       @@@; ;@@@@`                                  \n"
"   .@@@,        @@@#  @@@#@,                                \n"
"    ,@#@#        @@@@'  .@#@@#                              \n"
"      '@@@@'      `+@@@@  `;@@##                            \n"
"         #@@@@@@+,   .@@@@,  ,@@@@@@#                       \n"
"             :@@@@@@@.  .@@@`  #@@` ,#@@,                   \n"
"                   @@@@+   @@#  '@@@@@` +@; `               \n"
"                  @@'+@@@:   @@  +@@#@@@, #@ `              \n"
"                 @  @@@@@@@   @'  @@@;  '@ ,@               \n"
"                @ '@@@+@@@@+  :@  :@#@@@` @`:@              \n"
"               #+`@@ `@@@@@#`  @` `@@;  ;+`@ @@             \n"
"               @ @@  ,   `@@#  @' .' .#@   @':@`            \n"
"               @ @@  '@@@@@@@  @; #@@'  . `@@ @@@@;         \n"
"               @ @@+@@.   @@@:`@. @` '@@@, @+.@@@@@@        \n"
"               @@ @@ ##@@@@@@@,# @@@@@: `@#@ ## # #@@       \n"
"               +@` @@@@@@@@@@@@@#@@@@@@@@@#+`@@.@ @@@`      \n"
"              ` @@  +@@@@@@@@@@@@@@@@@@@@@` @@@@@@@@@@      \n"
"               '@@@:  +@@@@@@@@@@@@@@@@@'  #@@#  @@@@@      \n"
"             @@@@@ #@,  ,@@@@@@@@@@@@@,  @@@`@@  +'@+@      \n"
"           +@@# +@   @#@:```:'###+;. `'@@@@.+@@@##.@+@      \n"
"         ;@@;    @ @   +@@@@@@@@@@@@@@@@@@:`@:#@@@@ @.      \n"
"      ` @@+      @',      :@@@@@@+'@@@@@@@  @   @   #       \n"
"     `.@@   @@,  @@ +           #@@@@@@@@  ,@ ,@ ' @@@      \n"
"     +@#  +@@@.  `@ @             ,#@@@@@  @+@@   @@@@@`    \n"
"    #@@  @@@@     @@.@            @@@@@@@ .@@'   @@  ;@@    \n"
"   +@@  @@@@  #@  ;@ @`         ` `@@@@@` #@@` #@@  .`,@@   \n"
"  `@@  @@@#  @@@+  @#,@           @@@@@@ @@@@@@'`    # +#,  \n"
"  @@@ ,@@@  @@#``  #@ @#         `@@@@@ .@@@;`         `@@  \n"
"  @@. @@@  :@@ `' ` #@`@:        @@@@@# @:            # ;@  \n"
" `@@  @@@  @@: @@   @@`+@,      @@@@@@ @'``           @  @, \n"
" `@@  @@.  @@ #@     @@ #@@`    #@@@@ +'              @  @' \n"
"  @@  @@  .@+ @@     @@, ;@@@@@@@@@+ #'            @@ @  @+ \n"
"  @@; @@  ,@` @'      @@'` :#@@@@;  @:`           @@  @  @; \n"
"  @@@ '@; `@` @,      `@@@@.     ,@@             @@  @@ +@` \n"
"  @@@` @@  @` @:        :@@@@@@@@@,              @  @@` @@  \n"
"   #@@  @@ @+ @@                                @  @@@ '@+  \n"
"    @@@ ``@' '`'@                            +  @@@@  #@,   \n"
"      @@@`  ;@:  `@,                       +@@@@@` ,@#@     \n"
"        #@@@+ ;@  `@@@;              `'@@@@+    '@@@;       \n"
"            ;#@@@,     :@@@@@@@@@@@@@+:     :@@@@;          \n"
"               #@@@@#.                `;@@@@@@              \n"
"                  '@@@@@@@@@@###@@@@@@@@@@;                 \n"
"                      ,#@@@@@@@@@@@#',`                     \n"
"\n"
"\n"
"MADRAS doesn't make coffee for the moment, but we are working on it, it's on the TODO list.\n");
//*


/*
printf ( 
		  "                                   ... .=.? .                                                              \n"
		  "                                    .,=?~ =~                                                               \n"
		  "                                   :?=I?I+= ......                                                         \n"
		  "                                   =??,.=+=II???I,. . .  ..,:I                                             \n"
		  "                                   .::III7I,  :,7++?=, ...:~7..                                            \n"
		  "                                     .  .. ...:?I:I+?+, .:??,?.                                            \n"
		  "                                     . ....   .. :.???I~ .=II?..                                           \n"
		  "                                   ..  . . .  . =,+++I+, ...~+I+?:.. .                                     \n"
		  "                                   ..  . .. ..+?=????+ .  ..?+.I++I, .                                     \n"
		  "                                   :?I= ~~???+I?II=, . .  . .?? =+?+.                                      \n"
		  "                                   +?= ??? I?+., ...  .. .. ,=?  ?+?..                                     \n"
		  "                                   ??.??+??  ??? .      ....$+ =?+??                                       \n"
		  "                                   I+ I+???~.?~.     . .  . ???I+?+?.                                      \n"
		  "                                    ,I.,I???I?. .    =???.=I?~,,,. ..                                      \n"
		  "                                          .??+=.. .,=?I: ???..                                             \n"
		  "                               ..      .I??IIIIII?I,I??,.I.                                                \n"
		  "                          =NMMMMNDNDD8NMZZ$8??+ZOZZZ7+?+?ZZZZ7$ODM8DO:,+=+                                 \n"
		  "                      O8MMZZZZDNMMMMMMMMMNM8N8MMMMMMM7?I??7NMMMMMNNMZ7$I$$O8M8D..,.                        \n"
		  "                =~:ZO$7$$DN8ZZZOMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMZ?=ONDMND87$$$Z$NMM8.                      \n"
		  "            ?DDM77Z$$8NNMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM7I+I7MMNZ$7$$$Z8MO.                    \n"   
		  "            IMMM8I$$Z7$$88DMMMMMMMMMMMMMMMMMMMMMMMMMMMO$Z7====+??IOO8ZMMMMD8$$$Z778NMN.                    \n"     
		  "            IMMDZOMMDZZZ7$$7$$7$Z$$$ONNMMMMMMMMMMMMMMMMMMMMMMMMMMM$$ZZZ$Z7$$$$$MNMMMMD.                    \n"
		  "            IMMDZZZZZZ88NMNDDDZ77$$$$Z$$$$$$$$77$$$7$$$$$$$$ZZZ$$Z$$$$7$O8DMMND8OOZMMD.7777$               \n"
		  "             MMDZOZZOOZZ$ZZOZZ8NMMMMMMMMMMD$ZZZZZ$$Z$$$$$$ZDMMMMMMMMMMDOZZZ$ZZZZ$ZMMMMMMMMMMMNM+           \n"
		  "             MMDZ$$??7$OZZZZZOZZZZZZ$$ZZ$$ZD88D888888888888Z$Z$ZZ$$ZOZOOZOOZOOZZZZNND8DZZ$ZO8NMMMM         \n"
		  "             MMDZZZ?IIII?7I$ZZZZOZZZZZZZZZZ$ZZZZZZZZZZZZZZZOZZZZZZZZZZZOZOZZZZZZZZZZZ$ZOZZZ$OZZMMMM        \n"
		  "             MMDZZZIIII??IIIIIIIIIIII7$$$77II$OOZZZOZZZZZZZZZZZZZZZZZZZZZZZZZOOZZZOZZ$DDDDD$OZZO8NM$       \n"
		  "             NMN$ZZ$IIIIIIIIIIIIIIIIIII??777$ZZZZ$$ZZZZZZZZZZZZZZZZZZZZZOZZZZZZZ$MMMMMMMMMMMMZZZZ8MM       \n"
		  "             MMMZZZZ?IIIIIIII7ZZZZ$$$ZOOOZZOZZZZZZZOZZZZZZZZZZZZZZZZZZZZZZZZZOZZZMMMM:    ~MMNOZZOMM       \n"
		  "             MMM$ZZZ?IIIIIIIIIIIIIIII?IIIIII7$$$$$ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ$MMMZ      ,MMZ$ZOMM       \n"
		  "              MMZZZZ$I?IIIIIIIIIIIIIIIIIIIII??I7ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZOONMM:      ,MMZ$ZOMM       \n"
		  "              MNOZZZ$IIIIIIIIIIIIIIIIIIII7$ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZOZZ$8MMM       MMNZZZ8MM       \n"
		  "              IMM$ZZZIIIIIIIIIII7ZZOOOOZZZZ$ZZZZZZOZZZZZZZZZZZZZZZZZZZZZZZZZZZZONMMM      NMMOZZZNMM       \n"
		  "              =MMOZZO?IIIIIIIIIIIII??II???????????IZZZZZZZZZZZZZZZZZZZZZZZZZZZ$OMMMM     ~MM8ZZZNMM$       \n"
		  "               $MNOOOIIIIIIIIII?IIIIIIIIIII?IIZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZNMMMO   ~MMMDZZ8DMM=        \n"
		  "                MMDZZZIIIIIIIIIIIIIIII7$$$ZOZZZZOZZOZZZZZZZZZZZZZZZZZZZZZZZZZZZMMMN   $MMMD$ZZZMNM         \n"
		  "                MMN$ZZ$IIIIIIIIIIII7$ZZZZZZ$ZZOZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ$MMMMNMMNMN8ZZ$ZNMD.         \n"
		  "                ?MM$$ZZ7IIIIIIIIIIIIIIIII??????II??ZZZZZZZZZZZZZZZZZZZZZZZZZZZZNMMMMMNDZZZZ$NMM7,          \n"
		  "                 ~MNZZZZIIIIIIIIIIIIIIII?IIIIIII7ZZZZZZZZZZZZZZZZZZZZZZZZZZZOOOZZZZZOOZ$7ZNMMMO,           \n"
		  "                 ,ZMDZZZII?IIIIIIIIIIIIII?I$$ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZDNMMM               \n"
		  "                   MMZZZZ7?IIIIIIIIIII7$$$$$$$77$$7$7$ZZZZZZZZZZZZZZZZZZ$ZZZDNNMMMMMMMMM$~                 \n"
		  "                   ~MMN$ZZ$?IIIIIIIIIIIIIIIIIIIII?IZ$ZZZZZZZZZZZZZZZZZ$OZ$OZMMMMMMMMN:                     \n"
		  "                    ~MMDZZZ7IIIIIIIIIIIIIIIIIII7$OOZZZZZZZZZZZZZZZZZZZOZZZZNMMMMI                          \n"
		  "                     DMN8OOO$IIIIIIIIII7ZZZZZZZZZZZ$ZZZZZZZZZZZZZZZZZZZZONMMMMM,                           \n"
		  "                I$DMMMMMM8$ZZZ?IIIIIIIII77$$ZZOOOZ$$Z$$ZZ$ZZZZZZZZ$ZZ$ZZOMMMMMM:                           \n"
		  "          ,~DMMMMMMMMMMMMMNZ$ZZ$IIIIIIIIIII?IIIIIII7I$ZZZZZZZZZZZZOZZZZNMMMMMMMMMMMM+~~:~ .                \n"
		  " .. .,  $MMMMMMM$Z?,::=~7MMMO$ZZ$I?IIIIIIIIIIIIII?I7$$ZZZZZZZZZZZZZZO8MMMMMM$~:,~$ZMNNNMN7~,,.             \n"
		  ". ..+MMMMMDO.~~:~~~~~::,,,:MMMNOOZ$ZZZZ77II777ZZZ$ZZZZZZ$OZZZZZZZZONMMMMMM$:~~:~~:=~~~:8MMMMMMN=           \n"
		  "..8MNMMMM~~~~:~~=:7DNMM??I:=7MMMNDZZZ$ZZZZZZZZ$ZZZZZZZZZZZOZ$ZZ$8MMMMMMMN~~,~~~~~~~~~~~~:~MMMMMMMO,        \n"
		  ".ZMMMMMD~~:~~~:$MMMO,:~+++++=+MMMMMD8OZZZZZZZZZZZ$$OZZZZZZZZODMMMMMMMMNO~77???~=~~~~~::~~:~:MMMMMMM+.., ,.  \n"
		  " OMMMM$:~~:~~DMMMMZ~~+++++++?MMMMMMMMMMMNDO$$ZZZZZZZOZ$$ONMMMMMMMMMMN8++==$MDZ+?~~::~~~~~~=~,+MMMMMM888OO8OO8?. \n"
		  "7MMMMD:~:~,~~MMMM8,:?=?=+=++IMMMMMMMMMMMMMMNNNMMMMMMMMMMMMNNMMMMMMD7+++++++~MMN+++=~~~:?=ZNMMMMMMMMMMD88DDDMMMMMMM8~=   \n"
		  "=DMMMD,=~~~+=MMMMNI:+=+++++++=NMMMMMMMMMMMMMMMMMMMMMMMMMMMMNMMNI7++?=+=++?==MMM??+?+ZDMMMMM+I+=,,,..,.,ODDOO88OONMMMMI..\n"
		  ":ZMMMM$,=~~=::NNMMM,==?++++++++=DDMMMMMMMMMMMMMMM8O8888D8$:=+=+=++==+=== ?=8MMN++=$MM87,::,?+MNMMMNMMMMMMMMMMMMMMN?DDMM?\n"
		  ",~MMMMNM:~:~~~~~=:$MMM~++?+=?===++?+=?+=+I?+I8MNNMMMM8+++=?=+=?++++==+?7MMMMN8==7MD,~+NMMMMMMMMMMMMNMMMMMMMNMMZNNNIZ7MMM\n"
		  "..~=DMMMMN:~=:~:~~~:,77$=~=+??+===+++++?OOZZZ+===+=~========?ZOOO8MMMMMMMZ$I??+OM::~MMMMMMMMDMNMMMMMDMMDNMM$8NZIZNII$MMM\n"
		  "  .:~:7NMMMM$~:+~:~:~=~:~~~:::~=++++++++++~?=NMNNNNMMMMMMMNMNMMMMMNN??+?+==++MM,:~MMMMNMM$NM8$MNM7MMINM87MNZI88?7$?7$MMD\n"
		  ".. .,~=~:$MMMMMZ:,~~~:~~~~:~~~~::~~:~~==~++?????+?? ===+=++=+++==+:$8O7$DNMD77.ZOMMM$ZNM$7MM$$8M87MM$7MZ7$87$I77I=$ZMM+.\n"
		  ".     IZ:~::~DMMMMM==~:~~~~=~~~~~::~~~~~~:=~~:::::==:+~~8MMMMMMMMMDMMDI77I77MMMMMMMM77OZI$MM77$M77$NI77I77Z$$=+I7ZNMD...\n"
		  "........7MZZ+:~~,:$$MMZ$~,,~~~:~,:,,,:,$ZZZZMMMMMMMMNIIII7:....,.+++?7$I7III7778MMO7$7$7$$IN777O7$7I77I7Z7?+=7IDMMN,.   \n"
		  "...........MMMMM++=:,~+=~+~MMMMMMMMMMNNNI77,........,,,,~$$777$Z$Z8NMMMMMMMMNMMMM$7$II$$77$77$IIII77Z?+?+=77ONMMM\n"
		  "..,DD888MMMMMMMMOZZ$Z$$7~,,,,,,,....,,=IIII$7$7I$I7DMNMMMMMMMMM8=+=+?+?,:::,:::IMMMM7$7+~=+++=++???$7$ZNNNMMM,\n"
		  "7MDNZ7I77$Z7?I?+.,.....,~~:~=ZZ$Z$7777$OOZOMMMMMMMMOOO8+,,::~~~:~~=~~~~~~~.:.,:I7DMMMMO8O$7$$$OZ$ZDNMMMMMZ. \n"
		  "MOI7$,,:...,III7II7$7$I$77I$IIDMNNNMMMMMMMMMMMMNZ:,:::~~~~~::~~::~:ZN8DDNDMMMMMMDM~,NDMMMMMMMMNMMMMMN+,\n"
		  "MZ$$I777$7I$777I77ZZOONNMNNMMMMMDMMMMMMMMMMMMMMNMMMMMMMMMMMMMNNMMMNMNMMMMMNZZ?.........,$OZZZZ$:\n"
		  "MO77I77$7$MDNNMMMMMMMMMM+,~:........:~,~MMMMMMMMMMMMMMMMMMNMMMMMMMMM::::    \n"
		  "DMMMMMMMMMMMMZZ$$.   \n"
		  " ,NMMMMN:,    \n"
		  "\n"
		  "\n"
		  "\tMADRAS doesn't make coffee for the moment, but we are working on it, it's on the TODO list.\n");
*/
