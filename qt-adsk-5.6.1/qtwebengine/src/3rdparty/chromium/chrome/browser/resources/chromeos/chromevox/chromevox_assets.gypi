# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromevox_assets_chromevox': [
      'chromevox/chromevox-128.png',
      'chromevox/chromevox-16.png',
      'chromevox/chromevox-19.png',
      'chromevox/chromevox-48.png',
    ],
    'chromevox_assets_chromevox_background_earcons': [
      'chromevox/background/earcons/alert_modal.ogg',
      'chromevox/background/earcons/alert_nonmodal.ogg',
      'chromevox/background/earcons/bullet.ogg',
      'chromevox/background/earcons/busy_progress_loop.ogg',
      'chromevox/background/earcons/busy_working_loop.ogg',
      'chromevox/background/earcons/button.ogg',
      'chromevox/background/earcons/check_off.ogg',
      'chromevox/background/earcons/check_on.ogg',
      'chromevox/background/earcons/collapsed.ogg',
      'chromevox/background/earcons/editable_text.ogg',
      'chromevox/background/earcons/ellipsis.ogg',
      'chromevox/background/earcons/expanded.ogg',
      'chromevox/background/earcons/font_change.ogg',
      'chromevox/background/earcons/invalid_keypress.ogg',
      'chromevox/background/earcons/link.ogg',
      'chromevox/background/earcons/listbox.ogg',
      'chromevox/background/earcons/long_desc.ogg',
      'chromevox/background/earcons/new_mail.ogg',
      'chromevox/background/earcons/object_close.ogg',
      'chromevox/background/earcons/object_delete.ogg',
      'chromevox/background/earcons/object_deselect.ogg',
      'chromevox/background/earcons/object_enter.ogg',
      'chromevox/background/earcons/object_exit.ogg',
      'chromevox/background/earcons/object_open.ogg',
      'chromevox/background/earcons/object_select.ogg',
      'chromevox/background/earcons/paragraph_break.ogg',
      'chromevox/background/earcons/search_hit.ogg',
      'chromevox/background/earcons/search_miss.ogg',
      'chromevox/background/earcons/section.ogg',
      'chromevox/background/earcons/selection.ogg',
      'chromevox/background/earcons/selection_reverse.ogg',
      'chromevox/background/earcons/special_content.ogg',
      'chromevox/background/earcons/task_success.ogg',
      'chromevox/background/earcons/wrap_edge.ogg',
      'chromevox/background/earcons/wrap.ogg',
    ],
    'chromevox_assets_chromevox_background_keymaps': [
      'chromevox/background/keymaps/classic_keymap.json',
      'chromevox/background/keymaps/experimental.json',
      'chromevox/background/keymaps/flat_keymap.json',
    ],
    'chromevox_assets_chromevox_background_mathmaps_functions': [
      'chromevox/background/mathmaps/functions/algebra.json',
      'chromevox/background/mathmaps/functions/elementary.json',
      'chromevox/background/mathmaps/functions/hyperbolic.json',
      'chromevox/background/mathmaps/functions/trigonometry.json',
    ],
    'chromevox_assets_chromevox_background_mathmaps_symbols': [
      'chromevox/background/mathmaps/symbols/greek-capital.json',
      'chromevox/background/mathmaps/symbols/greek-mathfonts.json',
      'chromevox/background/mathmaps/symbols/greek-scripts.json',
      'chromevox/background/mathmaps/symbols/greek-small.json',
      'chromevox/background/mathmaps/symbols/greek-symbols.json',
      'chromevox/background/mathmaps/symbols/hebrew_letters.json',
      'chromevox/background/mathmaps/symbols/latin-lower-double-accent.json',
      'chromevox/background/mathmaps/symbols/latin-lower-normal.json',
      'chromevox/background/mathmaps/symbols/latin-lower-phonetic.json',
      'chromevox/background/mathmaps/symbols/latin-lower-single-accent.json',
      'chromevox/background/mathmaps/symbols/latin-mathfonts.json',
      'chromevox/background/mathmaps/symbols/latin-rest.json',
      'chromevox/background/mathmaps/symbols/latin-upper-double-accent.json',
      'chromevox/background/mathmaps/symbols/latin-upper-normal.json',
      'chromevox/background/mathmaps/symbols/latin-upper-single-accent.json',
      'chromevox/background/mathmaps/symbols/math_angles.json',
      'chromevox/background/mathmaps/symbols/math_arrows.json',
      'chromevox/background/mathmaps/symbols/math_characters.json',
      'chromevox/background/mathmaps/symbols/math_delimiters.json',
      'chromevox/background/mathmaps/symbols/math_digits.json',
      'chromevox/background/mathmaps/symbols/math_geometry.json',
      'chromevox/background/mathmaps/symbols/math_harpoons.json',
      'chromevox/background/mathmaps/symbols/math_non_characters.json',
      'chromevox/background/mathmaps/symbols/math_symbols.json',
      'chromevox/background/mathmaps/symbols/math_whitespace.json',
      'chromevox/background/mathmaps/symbols/other_stars.json',
    ],
  },
  'targets': [
    {
      'target_name': 'chromevox_assets',
      'type': 'none',
      'copies': [
        {
          'destination': '<(chromevox_dest_dir)/chromevox',
          'files': [
            '<@(chromevox_assets_chromevox)',
          ],
        },
        {
          'destination': '<(chromevox_dest_dir)/chromevox/background/earcons',
          'files': [
            '<@(chromevox_assets_chromevox_background_earcons)',
          ],
        },
        {
          'destination': '<(chromevox_dest_dir)/chromevox/background/keymaps',
          'files': [
            '<@(chromevox_assets_chromevox_background_keymaps)',
          ],
        },
        {
          'destination': '<(chromevox_dest_dir)/chromevox/background/mathmaps/functions',
          'files': [
            '<@(chromevox_assets_chromevox_background_mathmaps_functions)',
          ],
        },
        {
          'destination': '<(chromevox_dest_dir)/chromevox/background/mathmaps/symbols',
          'files': [
            '<@(chromevox_assets_chromevox_background_mathmaps_symbols)',
          ],
        },
      ],
    },
  ],
}
