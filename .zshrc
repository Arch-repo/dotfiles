[[ $- != *i* ]] && return

# ─── SSH SAFE MODE (Cloudflare fix) ─────────────────────────────
if [[ -n "$SSH_CONNECTION" ]]; then
    export TERM=xterm-256color
    export ZSH_SSH_SAFE=1
fi

# ─── Meta ───────────────────────────────────────────────────────
# Created by anto426
# Ensure running interactively
[[ $- != *i* ]] && return

# ─── History ─────────────────────────────────────────────────────
HISTSIZE=5000
HISTFILE=~/.zsh_history
SAVEHIST=$HISTSIZE
HISTDUP=erase
setopt appendhistory
setopt sharehistory

# ─── Keybinds ────────────────────────────────────────────────────
bindkey -e

# ─── FZF ─────────────────────────────────────────────────────────
if command -v fzf >/dev/null 2>&1; then
    eval "$(fzf --zsh)"
fi
# FZF theme, aligned dynamically with the desktop wallpaper palette
if [[ -f ~/.config/colors/colors.sh ]]; then
    source ~/.config/colors/colors.sh
    export FZF_DEFAULT_OPTS=" \
    --color=bg+:${ANTO426_SURFACE},bg:${ANTO426_BACKGROUND},spinner:${ANTO426_MUTED},hl:${ANTO426_ACCENT} \
    --color=fg:${ANTO426_FOREGROUND},header:${ANTO426_MUTED},info:${ANTO426_MUTED},pointer:${ANTO426_ACCENT} \
    --color=marker:${ANTO426_ACCENT},fg+:${ANTO426_FOREGROUND},prompt:${ANTO426_ACCENT},hl+:${ANTO426_ACCENT} \
    --color=selected-bg:${ANTO426_SELECT} \
    --color=border:${ANTO426_BORDER},label:${ANTO426_FOREGROUND}"
    export FZF_TAB_COLORS="fg:${ANTO426_FOREGROUND},bg:${ANTO426_BACKGROUND},hl:${ANTO426_ACCENT},min-height=5"
else
    # Fallback to static theme palette
    export FZF_DEFAULT_OPTS=" \
    --color=bg+:#7f635d,bg:#3e2d28,spinner:#c18c80,hl:#f38ba8 \
    --color=fg:#f6f7fb,header:#c18c80,info:#b9c4d2,pointer:#f9e2af \
    --color=marker:#a6e3a1,fg+:#ffffff,prompt:#c18c80,hl+:#f38ba8 \
    --color=selected-bg:#997a73 \
    --color=border:#98817b,label:#f6f7fb"
    export FZF_TAB_COLORS='fg:#f6f7fb,bg:#3e2d28,hl:#f38ba8,min-height=5'
fi

# ─── Zinit ───────────────────────────────────────────────────────
# Set the directory we want to store zinit and plugins
ZINIT_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}/zinit/zinit.git"
# Download Zinit, if it's not there yet
if [[ ! -d "$ZINIT_HOME" ]] && command -v git >/dev/null 2>&1; then
    mkdir -p "$(dirname "$ZINIT_HOME")"
    git clone https://github.com/zdharma-continuum/zinit.git "$ZINIT_HOME"
fi

if [[ -r "${ZINIT_HOME}/zinit.zsh" ]]; then
    source "${ZINIT_HOME}/zinit.zsh"

    # Add in zsh plugins
    zinit light zsh-users/zsh-syntax-highlighting
    zinit light zsh-users/zsh-completions
    zinit light zsh-users/zsh-autosuggestions
    zinit light Aloxaf/fzf-tab
fi

# ─── Completion ──────────────────────────────────────────────────
autoload -Uz compinit && compinit
(( $+functions[zinit] )) && zinit cdreplay -q

# Completion styling
zstyle ':completion:*' matcher-list 'm:{A-Za-z}={A-Za-z}'
zstyle ':completion:*' list-colors "${(s.:.)LS_COLORS}"
zstyle ':completion:*' menu no
zstyle ':fzf-tab:*' use-fzf-default-opts yes
zstyle ':fzf-tab:*' fzf-flags --height=17
zstyle ':fzf-tab:complete:*' fzf-preview '
if [ -d "$realpath" ]; then
    eza --icons --tree --level=2 --color=always "$realpath"
elif [ -f "$realpath" ]; then
    bat -n --color=always --line-range :500 "$realpath"
fi
'

# ─── Aliases ─────────────────────────────────────────────────────
alias ls='eza --icons --color=always'
alias ll='eza --icons --color=always -l'
alias la='eza --icons --color=always -a'
alias lla='eza --icons --color=always -la'
alias lt='eza --icons --color=always -a --tree --level=1'
alias grep='grep --color=always'
alias vim='nvim'
alias lzg='lazygit'
alias lzd='lazydocker'
alias cbonsai='cbonsai -l -i -w 1'

# ─── Python Venv Functions ─────────────────────────────────────────
_activate_venv() { 
    if [[ -f "$1/bin/activate" ]]; then
        source "$1/bin/activate"
    else
        echo "Error: Virtual environment not found at '$1'."
    fi
}

mkvenv() {
    local name="${1:-.venv}"
    if [[ ! -d "$name" ]]; then
        python3 -m venv "$name" || return 1
    fi

    _activate_venv "$name"
}

venv() { _activate_venv "${1:-.venv}" }
pwnvenv() { _activate_venv "$HOME/pwndbg/.venv" }

# ─── Tools Init ──────────────────────────────────────────────────
# Setup bat (better than cat)
export BAT_THEME="base16"
alias bat='bat --paging=never'

# Setup zoxide (better than cd)
if command -v zoxide >/dev/null 2>&1; then
    eval "$(zoxide init zsh)"
fi

# Allowing comments in interactive zsh commands
setopt interactivecomments


# Pokemon startup
#pokemon-colorscripts --no-title -s -r
if [[ -x "$HOME/neofetch-random.sh" ]]; then
    echo ""
    "$HOME/neofetch-random.sh"
fi
# Initialize Oh-My-Posh
if command -v oh-my-posh >/dev/null 2>&1; then
    eval "$(oh-my-posh init zsh --config ~/.config/ohmyposh/anto426.omp.json)"
fi

# bun completions
[ -s "$HOME/.bun/_bun" ] && source "$HOME/.bun/_bun"

# bun
export BUN_INSTALL="$HOME/.bun"
export PATH="$BUN_INSTALL/bin:$PATH"
export PATH="$HOME/.local/bin:$PATH"

# Android SDK
export ANDROID_HOME="$HOME/Android/Sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export PATH="$PATH:$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/emulator"
