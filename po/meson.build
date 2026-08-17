i18n = import('i18n')

# Note: this does not yet cover every translatable string. Strings that are
# not reachable from the C++ sources -- those in the Inno Setup installer
# script, the Lua automation scripts and the JSON resources -- still need
# tools/make_pot.sh to refresh the template.
i18n.gettext('aegisub',
             args: [
                 '-k_', '-kSTR_MENU', '-kSTR_DISP', '-kSTR_HELP', '-kwxT',
                 '-kfmt_tl', '-kfmt_plural:2,3', '-kwxPLURAL:1,2',
                 '--sort-by-file',
             ],
             install_dir: localedir)
