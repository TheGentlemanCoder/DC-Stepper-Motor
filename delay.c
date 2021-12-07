/****************************************************************************
 Author: Nicholas R. Smith
 Date Created: February 3, 2021
 Last Modified: February 3, 2021
 Description: A function to delay the microcontroller by an arbitrary
              number of milliseconds
 Inputs: Integer number of milliseconds
 Outputs: None
****************************************************************************/

// Subroutine to delay n milliseconds
// Inputs: Number of milliseconds to delay
// Outputs: None
void delayMs(int n) {
    int i, j;
    for(i = 0 ; i< n; i++)
        for(j = 0; j < 6265; j++)
            {}  /* do nothing for 1 ms */
}
