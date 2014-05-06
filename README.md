loopFindr
=========

This program finds loops in videos and allows the user to export them as gifs. 

I recommend using videos of a resolution no higher than 360p and .mp4 format. 
I have used clipconverter.cc to get youtube and vimeo videos with success.


Install:
=========
Get the .app from the zipped download of the repostitory, because I've made some changes to the addons that it requires as well as OpenFrameworks. I'll include some details as to those in the near future when I track them all down.


Usage Details:
=========
The parameters of the loop finding often need to be adjusted according to the video input.

The Loop Length Range setting sets the range of loop lengths for which the application looks.  

The Accuracy metric determines the % pixel difference required of the first and last frames of any loops found.
If the foreground is small then this needs to be set higher.

The Minimum Movement setting requires that there be at least some % pixel change throughout the loop.
This is automatically set to 1 so that most static images are not considered loops.

The Maximum Movement setting puts a limit on the % pixel change that can occur over a loop. 
This insures that loops do not have any discontinuties such as cuts, fades, or flashes.

