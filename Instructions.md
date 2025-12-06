Instructions how to use the Lilygo T-Watch-2020 V3 for hiking sessions and to send the
data via bluetooth to a Raspberry Pi.

1. Start receiver.py and wserver.py on the Raspberry Pi by doing the following:
	- open a terminal on the RPi
	- cd to the directory where receiver.py and wserver.py are
	- run the python file by writing command "python3 receiver.py"
	- do the same procedure for wserver.py

2. Make sure that the battery in the Lilygo T-Watch is charged before starting the hike
   session. If the screen is black push the button on the side of the watch to turn the
   display on.

3. Press the START button on the watch to start a hiking session.

4. Start hiking and see how the steps and distance increase. You can also press the side
   button again to turn off the display and save some battery charge.

5. When your hike is done press the STOP button. This will save the session data to the
   memory of the watch.

6. Now you can start a new hike -> last session data will be overwritten or send the data
   to the connected RPi by pressing the SEND button on the watch.

7. When you have sent over some data from the watch to the RPi you can open a browser
   window on the RPi and browse to "http://127.0.0.1:5000" to view the hiking sessions.

8. When you browse to "http://127.0.0.1:5000", the browser is going to take you to a site
   with a button View Sessions. Press this button to see all sessions that have been sent
   from the watch to the RPi.

9. The next window will show you all of the sessions that have been sent from the watch to
   the RPi. Press View to view the statistics for a session or delete a session by
   pressing the delete button.

10. You can also watch the demo_video in the same directory as these instructions to get
    a visual example on how to use the watch and RPi.