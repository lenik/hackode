# bash completion for enckode

_enckode()
{
	local cur prev words cword
	_init_completion || return

	if [[ $cur == -* ]]; then
		COMPREPLY=($(compgen -W '--dict --stdout --encrypt --decrypt --string --auto --number --verbose --quiet --help --version' -- "$cur"))
		return
	fi

	_filedir
}

complete -F _enckode enckode
