1. sudo apt-get install libmp3lame-dev

2. Input:
./mencoder input.mpg -o output.avi -oac mp3lame -ovc lavc -lavcopts threads=4