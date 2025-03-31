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