# Fonts

Only [BMF](http://bmf.php5.cz/) format is supported.

## Small font

You can replace default font by providing `SMALLFNT` lump. This font can provide lowercase characters.

Doom contains small font characters in form of `STCFN%03d` lumps. If no `SMALLFNT` is present, ACE Engine internally converts `STCFN%03d` to BMF format.

Beware: due to presence of bogus `STCFN121` in `doom2.wad` i have disabled lowercase characters for conversion!

### Size

Small font size dictates line spacing. Normally, this value is read from font file. Though you can override this value using `menu.font.height` mod options for your WAD.

For more info refer to [config](config.md).

## Colors

All basic ZDoom text colors are supported, though due to palette limitations, some might now look very good.

This is numerical list for [config](config.md) options:

0. BRICK
1. TAN
2. GRAY
3. GREEN
4. BROWN
5. GOLD
6. RED
7. BLUE
8. ORANGE
9. WHITE
10. YELLOW
11. BLACK
12. LIGHTBLUE
13. CREAM
14. OLIVE
15. DARKGREEN
16. DARKRED
17. DARKBROWN
18. PURPLE
19. DARKGRAY
20. CYAN
21. ICE
22. FIRE
23. SAPPHIRE
24. TEAL
