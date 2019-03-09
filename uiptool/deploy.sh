#!/bin/bash
# copy script
#curl -H "Expect:" --request POST --data-binary "@$1" 6.6.6.15/c/upload/$1
curl -0T $1 6.6.6.15/i/$1 --progress-bar