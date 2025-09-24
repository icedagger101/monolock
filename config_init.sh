#!/usr/bin/env bash
set -euo pipefail

APP_NAME="monolock"
CONFIG_DIR="${HOME}/.config/${APP_NAME}"
CONFIG_FILE="${CONFIG_DIR}/config.ini"
DEFAULT_ASCII_SRC="default_ascii.txt"
DEFAULT_ASCII_DEST="${CONFIG_DIR}/default_ascii.txt"

echo "--- ${APP_NAME} Setup ---"

if [[ -f "${CONFIG_FILE}" ]]; then
    echo "Configuration already exists at ${CONFIG_FILE}."
    exit 0
fi

if [[ ! -f "${DEFAULT_ASCII_SRC}" ]]; then
    echo "Error: Missing '${DEFAULT_ASCII_SRC}'. Place it next to this script."
    exit 1
fi

mkdir -p "${CONFIG_DIR}"
cp "${DEFAULT_ASCII_SRC}" "${DEFAULT_ASCII_DEST}"

cat > "${CONFIG_FILE}" << EOF
#
# Configuration file for monolock
#
# Place this file at ~/.config/monolock/config.ini
# All colors are in 6-digit hex format, e.g., #RRGGBB
#

[Appearance]
# --- Main Look & Feel ---

# Fonts are specified in Xft format (Font_Name:size=NN).
# Examples: "Terminus:size=14", "JetBrains Mono:size=12", "monospace:size=16"
font = DejaVu Sans Mono:size=14

# Default color for text, cursor, and the input box border.
text_color = #cccccc

# Fill color for the password input box.
box_color = #000000

# Border color for the input box when the wrong password is entered.
error_color = #ff3333

# The character used to mask the password.
# You can use a standard asterisk '*' or something like a dot '•'.
password_char = *

# Background color for the entire screen.
background_color = #000000

[ASCII Art]
# --- Centerpiece Image Settings ---

# Path to the text file with your ASCII art.
# An absolute path is recommended. Tilde (~) is not expanded here.
# If the file is not found or this option is commented out, a fallback text will be used.
ascii_file = ${DEFAULT_ASCII_DEST}

# --- ASCII Art Color ---
#
# You have two options:
# 1. A vertical gradient using 'ascii_color_start' and 'ascii_color_end'.
# 2. A single solid color using 'ascii_color'.
#
# If gradient colors are set, 'ascii_color' will be ignored.

# Option 1: Vertical Gradient (Top to Bottom)
# Start color of the gradient (top part of the art).
ascii_color_start = #FFCEE6
# End color of the gradient (bottom part of the art).
ascii_color_end = #E56AB3

# Option 2: Solid Color
# Uncomment this line if you want to use a single color for the entire art.
# ascii_color = #ffffff

EOF

echo "Setup complete: ${CONFIG_FILE} created."
