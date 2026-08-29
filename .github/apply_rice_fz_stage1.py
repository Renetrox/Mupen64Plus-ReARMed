from pathlib import Path

# ---------------------------------------------------------------------------
# RiceConfig.cpp: reconnect the FZ per-ROM INI path that libretro compiled out
# ---------------------------------------------------------------------------
p = Path('gles2rice/src/RiceConfig.cpp')
s = p.read_text()

old_globals = "//std::vector<IniSection> IniSections;\n//bool    bIniIsChanged = false;\n//char    szIniFileName[300];"
new_globals = "std::vector<IniSection> IniSections;\nbool    bIniIsChanged = false;\nchar    szIniFileName[300];"
if old_globals not in s:
    raise RuntimeError('Rice INI globals marker not found')
s = s.replace(old_globals, new_globals, 1)

# The modern libretro copy commented the forward declaration when it disabled
# the INI path. Restore it before Ini_GetRomOptions calls FindIniEntry().
old_find_decl = "//static int FindIniEntry(uint32_t dwCRC1, uint32_t dwCRC2, uint8_t nCountryID, char* szName, int PrintInfo);"
new_find_decl = "static int FindIniEntry(uint32_t dwCRC1, uint32_t dwCRC2, uint8_t nCountryID, char* szName, int PrintInfo);"
if old_find_decl not in s:
    raise RuntimeError('FindIniEntry forward declaration marker not found')
s = s.replace(old_find_decl, new_find_decl, 1)

old_load = """bool LoadConfiguration(void)
{
#if 0
    IniSections.clear();
    bIniIsChanged = false;
    strcpy(szIniFileName, INI_FILE);

    if (!ReadIniFile())
    {
        DebugMessage(M64MSG_ERROR, \"Unable to read ini file from disk\");
        return false;
    }
#endif

    if (l_ConfigVideoGeneral == NULL || l_ConfigVideoRice == NULL)"""
new_load = """bool LoadConfiguration(void)
{
    IniSections.clear();
    bIniIsChanged = false;
    strcpy(szIniFileName, INI_FILE);

    /* Mupen64Plus FZ loads RiceVideoLinux.ini before applying per-ROM
     * settings. The libretro fork had this path compiled out, which made
     * every Rice INI hack a no-op. Keep the file optional for libretro:
     * when it is absent from RetroArch's system directory Rice continues
     * with plugin defaults instead of failing content startup. */
    if (!ReadIniFile())
        DebugMessage(M64MSG_WARNING,
                     \"Rice: %s not found in the libretro system directory; per-ROM Rice hacks are disabled\",
                     INI_FILE);

    if (l_ConfigVideoGeneral == NULL || l_ConfigVideoRice == NULL)"""
if old_load not in s:
    raise RuntimeError('LoadConfiguration disabled INI block not found')
s = s.replace(old_load, new_load, 1)

# Re-enable Ini_GetRomOptions only. Ini_StoreRomOptions remains disabled so a
# libretro core never rewrites the user's compatibility database.
a = s.index('void Ini_GetRomOptions(LPGAMESETTING pGameSetting)')
b = s.index('\nvoid Ini_StoreRomOptions(LPGAMESETTING pGameSetting)', a)
block = s[a:b]
if '#if 0' not in block or '#endif' not in block:
    raise RuntimeError('Ini_GetRomOptions is not in expected disabled form')
block = block.replace('#if 0\n', '', 1)
last = block.rfind('#endif\n')
if last < 0:
    raise RuntimeError('Ini_GetRomOptions closing #endif not found')
block = block[:last] + block[last + len('#endif\n'):]
s = s[:a] + block + s[b:]

# Re-enable the legacy read/parser implementation. Writing remains unused.
marker = '#if 0\nchar * left(const char * src, int nchars)'
if marker not in s:
    raise RuntimeError('Rice INI parser outer guard not found')
s = s.replace(marker, 'char * left(const char * src, int nchars)', 1)

end_marker = '\n#endif\n\nGameSetting g_curRomInfo;'
if end_marker not in s:
    raise RuntimeError('Rice INI parser closing guard not found')
s = s.replace(end_marker, '\n\nGameSetting g_curRomInfo;', 1)

push_guard = '#if 0\n                IniSections.push_back(newsection);\n#endif'
if push_guard not in s:
    raise RuntimeError('IniSections push guard not found')
s = s.replace(push_guard, '                IniSections.push_back(newsection);', 1)

nested_start = '#if 0\n                int sectionno = IniSections.size() - 1;'
if nested_start not in s:
    raise RuntimeError('Rice section parser nested guard not found')
s = s.replace(nested_start, '                int sectionno = IniSections.size() - 1;', 1)

nested_end = """                if (strcasecmp(left(readinfo,19), \"ScreenUpdateSetting\")==0)
                    IniSections[sectionno].dwScreenUpdateSetting = strtol(right(readinfo,1),NULL,10);
#endif"""
if nested_end not in s:
    raise RuntimeError('Rice section parser nested closing guard not found')
s = s.replace(nested_end, """                if (strcasecmp(left(readinfo,19), \"ScreenUpdateSetting\")==0)
                    IniSections[sectionno].dwScreenUpdateSetting = strtol(right(readinfo,1),NULL,10);""", 1)

# The custom char-buffer getline is defined below ReadIniFile. It needs a
# declaration in modern C++ or overload resolution falls through to stdio/std.
old_getline_decl = '//std::ifstream& getline( std::ifstream &is, char *str );'
new_getline_decl = 'std::ifstream& getline(std::ifstream &is, char *str);'
if old_getline_decl not in s:
    raise RuntimeError('Rice getline forward declaration marker not found')
s = s.replace(old_getline_decl, new_getline_decl, 1)

# __cdecl is a stale Windows-only declaration and Debugger.h already owns the
# real declaration. Remove this duplicate so GCC/Clang accept the restored code.
s = s.replace('void __cdecl DebuggerAppendMsg (const char * Message, ...);\n\nstatic int FindIniEntry',
              'static int FindIniEntry', 1)

# Match FZ's CR/LF trimming. The disabled libretro copy checked LF twice.
s = s.replace("(*p == ' ' || *p == 0xa || *p == '\\n')",
              "(*p == ' ' || *p == '\\r' || *p == '\\n')", 1)

# ConfigGetSharedDataFilepath may legally fail if the frontend has no system
# directory. Do not pass NULL to iostream::open().
old_path = """    const char *ini_filepath = ConfigGetSharedDataFilepath(szIniFileName);

    DebugMessage(M64MSG_VERBOSE, \"Reading .ini file: %s\", ini_filepath);
    inifile.open(ini_filepath);"""
new_path = """    const char *ini_filepath = ConfigGetSharedDataFilepath(szIniFileName);
    if (ini_filepath == NULL)
        return false;

    DebugMessage(M64MSG_VERBOSE, \"Reading .ini file: %s\", ini_filepath);
    inifile.open(ini_filepath);"""
if old_path not in s:
    raise RuntimeError('Rice INI pathname block not found')
s = s.replace(old_path, new_path, 1)

# FZ applies these tri-state ROM values (0 = plugin default, 1/2 = explicit
# false/true after decrement). libretro disabled the block after changing the
# target fields to bool. Config.h below restores integer storage first.
tri = """#if 0
    if( currentRomOptions.bNormalCombiner == 0 )            currentRomOptions.bNormalCombiner = defaultRomOptions.bNormalCombiner;
    else currentRomOptions.bNormalCombiner--;
    if( currentRomOptions.bNormalBlender == 0 )             currentRomOptions.bNormalBlender = defaultRomOptions.bNormalBlender;
    else currentRomOptions.bNormalBlender--;
    if( currentRomOptions.bFastTexCRC == 0 )                currentRomOptions.bFastTexCRC = defaultRomOptions.bFastTexCRC;
    else currentRomOptions.bFastTexCRC--;
    if( currentRomOptions.bAccurateTextureMapping == 0 )    currentRomOptions.bAccurateTextureMapping = defaultRomOptions.bAccurateTextureMapping;
    else currentRomOptions.bAccurateTextureMapping--;
#endif"""
tri_on = """    if( currentRomOptions.bNormalCombiner == 0 )            currentRomOptions.bNormalCombiner = defaultRomOptions.bNormalCombiner;
    else currentRomOptions.bNormalCombiner--;
    if( currentRomOptions.bNormalBlender == 0 )             currentRomOptions.bNormalBlender = defaultRomOptions.bNormalBlender;
    else currentRomOptions.bNormalBlender--;
    if( currentRomOptions.bFastTexCRC == 0 )                currentRomOptions.bFastTexCRC = defaultRomOptions.bFastTexCRC;
    else currentRomOptions.bFastTexCRC--;
    if( currentRomOptions.bAccurateTextureMapping == 0 )    currentRomOptions.bAccurateTextureMapping = defaultRomOptions.bAccurateTextureMapping;
    else currentRomOptions.bAccurateTextureMapping--;"""
if tri not in s:
    raise RuntimeError('Disabled FZ tri-state ROM block not found')
s = s.replace(tri, tri_on, 1)

p.write_text(s)

# ---------------------------------------------------------------------------
# Config.h: restore FZ's tri-state-capable storage in RomOptions
# ---------------------------------------------------------------------------
h = Path('gles2rice/src/Config.h')
t = h.read_text()
old_fields = """    bool    bNormalCombiner;
    bool    bNormalBlender;
    bool    bFastTexCRC;
    bool    bAccurateTextureMapping;"""
new_fields = """    uint32_t  bNormalCombiner;
    uint32_t  bNormalBlender;
    uint32_t  bFastTexCRC;
    uint32_t  bAccurateTextureMapping;"""
if old_fields not in t:
    raise RuntimeError('Rice RomOptions tri-state fields not found')
t = t.replace(old_fields, new_fields, 1)
h.write_text(t)
