/*
 * square5.c
 * this program lets you move the square, with vsync
 */

/* the width and height of the screen */
#define WIDTH 240
#define HEIGHT 160

/* these identifiers define different bit positions of the display control */
#define MODE4 0x0004
#define BG2 0x0400

/* this bit indicates whether to display the front or the back buffer
 * this allows us to refer to bit 4 of the display_control register */
#define SHOW_BACK 0x10

/* the screen is simply a pointer into memory at a specific address this
 *  * pointer points to 16-bit colors of which there are 240x160 */
volatile unsigned short *screen = (volatile unsigned short *)0x6000000;

/* the display control pointer points to the gba graphics register */
volatile unsigned long *display_control = (volatile unsigned long *)0x4000000;

/* the address of the color palette used in graphics mode 4 */
volatile unsigned short *palette = (volatile unsigned short *)0x5000000;

/* pointers to the front and back buffers - the front buffer is the start
 * of the screen array and the back buffer is a pointer to the second half */
volatile unsigned short *front_buffer = (volatile unsigned short *)0x6000000;
volatile unsigned short *back_buffer = (volatile unsigned short *)0x600A000;

/* the button register holds the bits which indicate whether each button has
 * been pressed - this has got to be volatile as well
 */
volatile unsigned short *buttons = (volatile unsigned short *)0x04000130;

unsigned int frameCounter = 0;

/* the bit positions indicate each button - the first bit is for A, second for
 * B, and so on, each constant below can be ANDED into the register to get the
 * status of any one button */
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)
#define BUTTON_R (1 << 8)
#define BUTTON_L (1 << 9)

/* the scanline counter is a memory cell which is updated to indicate how
 * much of the screen has been drawn */
volatile unsigned short *scanline_counter = (volatile unsigned short *)0x4000006;

/* wait for the screen to be fully drawn so we can do something during vblank */
void wait_vblank()
{
    /* wait until all 160 lines have been updated */
    while (*scanline_counter < 160)
    {
    }
}

/* this function checks whether a particular button has been pressed */
unsigned char button_pressed(unsigned short button)
{
    /* and the button register with the button constant we want */
    unsigned short pressed = *buttons & button;

    /* if this value is zero, then it's not pressed */
    if (pressed == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/* keep track of the next palette index */
int next_palette_index = 0;

/*
 * function which adds a color to the palette and returns the
 * index to it
 */
unsigned char add_color(unsigned char r, unsigned char g, unsigned char b)
{
    unsigned short color = b << 10;
    color += g << 5;
    color += r;

    /* add the color to the palette */
    palette[next_palette_index] = color;

    /* increment the index */
    next_palette_index++;

    /* return index of color just added */
    return next_palette_index - 1;
}

/* a colored square */
struct rectangle
{
    unsigned short x, y, height, width;
    unsigned char color;
};

// Move CPU paddle according to ball y value
void handle_cpu(struct rectangle *cpu, struct rectangle *ball)
{
    // Move down if ball is below paddle, but stop at bottom
    if (ball->y > cpu->y && cpu->y + cpu->height < HEIGHT)
    {
        cpu->y++;
    }
    // Move up if ball is above paddle, but stop at top
    else if (ball->y < cpu->y && cpu->y > 0)
    {
        cpu->y--;
    }
}

/* put a pixel on the screen in mode 4 */
void put_pixel(volatile unsigned short *buffer, int row, int col, unsigned char color)
{
    /* find the offset which is the regular offset divided by two */
    unsigned short offset = (row * WIDTH + col) >> 1;

    /* read the existing pixel which is there */
    unsigned short pixel = buffer[offset];

    /* if it's an odd column */
    if (col & 1)
    {
        /* put it in the left half of the short */
        buffer[offset] = (color << 8) | (pixel & 0x00ff);
    }
    else
    {
        /* it's even, put it in the left half */
        buffer[offset] = (pixel & 0xff00) | color;
    }
}

/* draw a square onto the screen */
void draw_rectangle(volatile unsigned short *buffer, struct rectangle *s)
{
    short row, col;
    /* for each row of the square */
    for (row = s->y; row < (s->y + s->height); row++)
    {
        /* loop through each column of the square */
        for (col = s->x; col < (s->x + s->width); col++)
        {
            /* set the screen location to this color */
            put_pixel(buffer, row, col, s->color);
        }
    }
}



/* clear the screen right around the square */
void update_screen(volatile unsigned short *buffer, unsigned short color, struct rectangle *s)
{
    short row, col;
    for (row = s->y - 3; row < (s->y + s->height + 3); row++)
    {
        for (col = s->x - 3; col < (s->x + s->width + 3); col++)
        {
            put_pixel(buffer, row, col, color);
        }
    }
}

/* this function takes a video buffer and returns to you the other one */
volatile unsigned short *flip_buffers(volatile unsigned short *buffer)
{
    /* if the back buffer is up, return that */
    if (buffer == front_buffer)
    {
        /* clear back buffer bit and return back buffer pointer */
        *display_control &= ~SHOW_BACK;
        return back_buffer;
    }
    else
    {
        /* set back buffer bit and return front buffer */
        *display_control |= SHOW_BACK;
        return front_buffer;
    }
}

/* handle the buttons which are pressed down */
void handle_buttons(struct rectangle *s)
{
    // Move paddle down, stop before screen bottom
    if (button_pressed(BUTTON_DOWN) && s->y + s->height < HEIGHT)
    {
        s->y += 1;
    }

    // Move paddle up, but stop before screen height
    if (button_pressed(BUTTON_UP) && s->y > 0)
    {
        s->y -= 1;
    }
}

// detects intersections, returns 1 if rectangles overlap
int intersects(struct rectangle *a, struct rectangle *b)
{
    int a_right  = a->x + a->width;
    int a_bottom = a->y + a->height;
    int b_right  = b->x + b->width;
    int b_bottom = b->y + b->height;

    // Return true (1) if rectangles overlap, false (0) if they don't
    return !(a_right  < b->x      ||  // a is completely left of b
             a->x     > b_right   ||  // a is completely right of b
             a_bottom < b->y      ||  // a is completely above b
             a->y     > b_bottom);    // a is completely below b
}

// CHANGE
void draw_ball(volatile unsigned short *buffer, struct rectangle *ball, struct rectangle *paddle, struct rectangle *cpu, signed short *dx, signed short *dy)
{
    short row, col; // for drawing the ball
    

    // initial update
    ball->x += *dx;
    ball->y += *dy;

    // Ball hitting side walls
    if (ball->x <= 0) { // left wall
        ball->x = 0;
        *dx = -*dx;
    } else if (ball->x + ball->width >= WIDTH) { // right wall
        ball->x = WIDTH - ball->width;
        *dx = -*dx;
    }

    // Ball hitting top 
    if (ball->y <= 0) {
        ball->y = 0;
        *dy = -*dy;
    } else if (ball->y + ball->height >= HEIGHT) { // ball hitting bottom
        ball->y = HEIGHT - ball->height;
        *dy = -*dy;
    }

    // Bounce off player paddle
    if (intersects(ball, paddle)) {
        *dx = 1;
    }

    // Bounce off cpu paddle
    if (intersects(ball, cpu)) {
        *dx = -1;
    }

    // Draw new ball
    for (row = ball->y; row < (ball->y + ball->height); row++) { // loop over rows
        for (col = ball->x; col < (ball->x + ball->width); col++) { // loop over collums 
            put_pixel(buffer, row, col, ball->color);
        }
    }
}



/* clear the screen to black */
void clear_screen(volatile unsigned short *buffer, unsigned short color)
{
    unsigned short row, col;
    /* set each pixel black */
    for (row = 0; row < HEIGHT; row++)
    {
        for (col = 0; col < WIDTH; col++)
        {
            put_pixel(buffer, row, col, color);
        }
    }
}

int checkVictory(struct rectangle *ball){
    if (ball->x <= 10){ // Player win
        return 0;
    } else if (ball->x >= 230){ // CPU win
        return 1;
    } else {
        return 2;
    }
}


void resetGame(unsigned char black, struct rectangle *ball, struct rectangle *player, struct rectangle *cpu, signed short *ball_dx, signed short *ball_dy, unsigned short *cpuHandicap) {
    
    ball->x = 120;
    ball->y = 80;

    // Found a rapidly changing byte to randomize off of
    unsigned short randVal = *scanline_counter;

    // Randomize direction based on bits of randVal
    if (frameCounter & 1){
        *ball_dx = 1;
    } else {
        *ball_dx = -1;
    }

    if (frameCounter & 2){
        *ball_dy = 1;
    } else {
        *ball_dy = -1;
    }

    clear_screen(front_buffer, black);
    clear_screen(back_buffer, black);

    player->y = 70;
    cpu->y = 70;

    *cpuHandicap = 0;
}

void draw_score(volatile unsigned short *buffer, unsigned short playerScore, unsigned short cpuScore, unsigned char playerColor, unsigned char cpuColor){
    struct rectangle tally;
    unsigned short i;

    // Draw player tallies on the left
    for (i = 0; i < playerScore; i++) {
        tally.x = WIDTH - 10 - (i + 1) * 8;
        tally.y = 5;
        tally.width = 5;
        tally.height = 3;
        tally.color = playerColor;
        draw_rectangle(buffer, &tally);
    }

    // Draw CPU tallies on the right
    for (i = 0; i < cpuScore; i++) {
        tally.x = 10 + i * 8;     // spacing between tallies, 10
        tally.y = 5;
        tally.width = 5;
        tally.height = 3;
        tally.color = cpuColor;
        draw_rectangle(buffer, &tally);
    }
}

void draw_net(volatile unsigned short *buffer, unsigned char white) {
    int x = WIDTH / 2; // middle of screen x value
    for (int y = 0; y < HEIGHT; y += 8) {  // loop from top to bottom of screen skipping over 8 pixels every time
        for (int i = 0; i < 4; i++) {      // loop 4 times
            put_pixel(buffer, x, y + i, white); // place pixel at y + i
        }
    }
}

/* the main function */
int main()
{
    /* we set the mode to mode 4 with bg2 on */
    *display_control = MODE4 | BG2;
    
    signed short dx = -1;
    signed short dy = -1;
    unsigned short playerScore = 0;
    unsigned short cpuScore = 0;

    /* make a green square */
    struct rectangle player_rect = {10, 60, 30, 5, add_color(0, 20, 2)};
    struct rectangle cpu_rect = {220, 60, 30, 5, add_color(20, 5, 0)};
    struct rectangle ball_rect = {120, 80, 2, 2, add_color(31, 31, 31)};

    /* add black to the palette */
    unsigned char black = add_color(0, 0, 0);
    unsigned char green = add_color(0,200,0);
    unsigned char white = add_color(255,255,255);

    /* the buffer we start with */
    volatile unsigned short *buffer = front_buffer;

    /* clear whole screen first */
    clear_screen(front_buffer, black);
    clear_screen(back_buffer, black);
    unsigned short cpuHandicap= 1; // boolean 
    unsigned short lastVictory = 2;  // 2 = ongoing, 0 = player, 1 = cpu
    
    /* loop forever */
    while (1)
    {
        frameCounter++;
        /* clear the screen - only the areas around the square! */
        update_screen(buffer, black, &player_rect);
        update_screen(buffer, black, &cpu_rect);
        update_screen(buffer, black, &ball_rect);
        handle_cpu(&cpu_rect, &ball_rect);

        // if (cpuHandicap == 1){
        //     cpuHandicap = 0;
        //     handle_cpu(&cpu_rect, &ball_rect);
        // } else {
        //     cpuHandicap = 1;
        // }

        unsigned short result = checkVictory(&ball_rect);

        if (result != 2 && result != lastVictory) {
            if (result == 0) {
                playerScore++;
            } else if (result == 1) {
                cpuScore++;
            }

            resetGame(black, &ball_rect, &player_rect, &cpu_rect, &dx, &dy, &cpuHandicap);
            lastVictory = result;  // remember who won
        } 
        else if (result == 2) {
            lastVictory = 2; // reset back to "no victory" once ball is in play
        }

        /* draw the square */
        draw_net(buffer, white);
        draw_rectangle(buffer, &player_rect);
        draw_rectangle(buffer, &cpu_rect);
        draw_ball(buffer,&ball_rect, &player_rect, &cpu_rect, &dx,&dy);
        draw_score(buffer, playerScore, cpuScore, white, green);
        
        

        /* handle button input */
        handle_buttons(&player_rect);

        /* wiat for vblank before switching buffers */
        wait_vblank();

        /* swap the buffers */
        buffer = flip_buffers(buffer);
    }
}
