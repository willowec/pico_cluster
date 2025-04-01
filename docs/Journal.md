# RPi Pico Cluster Journal
Willow Cunningham

This is a journal to contain my thoughts and todos during the development process. 
Read at your own risk.

## 3/30/2025

Got head pico to at least see compute pico on i2c using the i2c_slave library.
Very cool.

Next up, I want to be able to set the address with GPIO - I need at least 4 slave address configs, so 2 GPIO... done.

Next, I want to standardize communications. 
Send data, receive data, etc.
How to do.

Hmm, honestly it might be easier to start with sending image data to the pico from serial, right. Lets do that first.

Okay, got the data sending over. Image + kernel. ez as pie! Might go to bed now...

## 4/1/2025

Copied in the convolve code from class hw to shared, and convolve on the head. 
Sending data back to client using super primitive printf %d, because bytes were being annoying.
This is also being annoying tho - often fails to send all the bytes.

Still, sometimes an image makes it back and we can imshow in the client.
Unfortunately the result is always incorrect... something to work on tomorrow, its late