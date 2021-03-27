#!/usr/bin/env bash

SESSION_NAME="coffee_assistant"

if ! hash tmux 2>/dev/null; then
	echo "Install tmux first..."
	exit 1
fi

function has-session {
	tmux has-session -t $1 2>/dev/null
}

function get-shell {
	getent passwd $USER | awk -F: '{ print $7 }'
}

if [ "$1" == "kill" ]; then
	echo "Killing tmux"
	tmux kill-server
	exit 0
fi


SHELL="`get-shell`"
echo $SHELL

if has-session $SESSION_NAME ; then
	echo "Session $SESSION_NAME already exists"
else
        echo "Creating new session $SESSION_NAME"
        tmux new-session -d -s $SESSION_NAME -n $SESSION_NAME
        tmux split-window -h
        tmux select-pane -t 0
        tmux split-window -h
        tmux select-pane -t 0
        tmux split-window -v -p 20
        tmux select-pane -t 2
        tmux split-window -v -p 20
        tmux select-pane -t 4

        #+-----------------------+
        #| 0     |watch  | shell |
        #|       |       |       |
        #|-------|-------|       |
        #| dmesg | open  |       |
        #|       | ocd   |       |
        #+-----------------------+

        tmux send-keys -t $SESSION_NAME.0 "" Enter
        sleep 0.1

        tmux send-keys -t $SESSION_NAME.1 "dmesg -w" Enter Enter
        sleep 0.1

        tmux send-keys -t $SESSION_NAME.2 "" Enter Enter
        sleep 0.1

        tmux send-keys -t $SESSION_NAME.3 "" Enter Enter
        sleep 0.1

        tmux send-keys -t $SESSION_NAME.4 "FLASH=true MONITOR=true ./watch.sh skeleton" Enter Enter
        sleep 0.1
fi

tmux attach-session -d -t $SESSION_NAME
