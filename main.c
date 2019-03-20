// Compile for Emscripten with
//   emcc  -s USE_SDL=2 -o main.{html,c}
// Compile for native with
//   cc -o main{,.c} -I/usr/include/SDL2 -lSDL2 -lm

// libSDL2

#include <SDL.h>
#include <SDL_events.h>
#include <SDL_render.h>

// Platform (Emscripten or POSIX)

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

// C Standard Library

#include <stdio.h>

//-----------------------------------------------------------------------------

const float G = 1.0f;

//-----------------------------------------------------------------------------

typedef struct {
	int n;  // Length of arrays.
	float *x,  *y;
	float *vx, *vy;
	float *diam;
	SDL_Renderer *renderer;
	SDL_Texture **textures;
	int stepFrames;  // -1: keep running; 0: paused; N>0: step N frames.
} context_t;

void initContext (context_t *c, int n)
{
	c->n        = n;
	c->x        = malloc (n * sizeof(float));
	c->y        = malloc (n * sizeof(float));
	c->vx       = malloc (n * sizeof(float));
	c->vy       = malloc (n * sizeof(float));
	c->diam     = malloc (n * sizeof(float));
	c->renderer = 0;
	c->textures = malloc (n * sizeof(SDL_Texture*));
	c->stepFrames = -1;
}

void reinitContext (context_t *c, int n)
{
	c->n        = n;
	c->x        = realloc (c->x       , n * sizeof(float));
	c->y        = realloc (c->y       , n * sizeof(float));
	c->vx       = realloc (c->vx      , n * sizeof(float));
	c->vy       = realloc (c->vy      , n * sizeof(float));
	c->diam     = realloc (c->diam    , n * sizeof(float));
	// c->renderer = ...
	c->textures = realloc (c->textures, n * sizeof(SDL_Texture*));
	c->stepFrames = -1;
}

void destroyContext (context_t *c)
{
	free (c->x);
	free (c->y);
	free (c->vx);
	free (c->vy);
	free (c->diam);
	c->renderer = 0;
	free (c->textures);
}

//-----------------------------------------------------------------------------

void drawBody (SDL_Renderer *renderer, int diam)
{
	const float
		r = diam / 2.0f,  // Also the center x and y coord.
		r2 = r * r;

	for (int x = 0; x < diam; ++x) {	// So un-optimized!
		const float
			dx = x + 0.5f - r,
			x2 = dx * dx;

		for (int y = 0; y < diam; ++y) {
			const float
				dy = y + 0.5f - r,
				d = sqrtf (x2 + dy * dy) - r,
				scale =
					d >  1.0f ? 0.0f :
					d < -1.0f ? 1.0f :
					(1.0f - d) / 2.0f;
			const int rgb = 0.5f * scale * 255.0f;

			SDL_SetRenderDrawColor (renderer, rgb, rgb, rgb, 0x7f);
			SDL_RenderDrawPoint (renderer, x, y);
		}
	}
}

//-----------------------------------------------------------------------------

// Call this after setting diam[].
void createTextures (context_t *c)
{
	for (int i = 0; i < c->n; ++i) {
		SDL_Texture *texture = SDL_CreateTexture (
			c->renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET, c->diam[i], c->diam[i]);

		SDL_SetRenderTarget (c->renderer, texture);
		SDL_SetRenderDrawColor (c->renderer, 0xc0, 0xc0, 0xc0, 0xc0);
		drawBody (c->renderer, c->diam[i]);
		c->textures[i] = texture;
	}
}

void destroyTextures (int n, SDL_Texture** textures)
{
	for (int i = 0; i < n; ++i)
		SDL_DestroyTexture (textures[i]);
}

//-----------------------------------------------------------------------------

#ifndef EMSCRIPTEN

int continueMainLoop;

void cancelMainLoop() { continueMainLoop = 0; }

void mainLoop (void (*fn)(void *context), void *context, int sleepMs)
{
	for ( continueMainLoop = 1; continueMainLoop; SDL_Delay (sleepMs) )
		(*fn)(context);
}

#endif

//-----------------------------------------------------------------------------

enum UserInput {
	USER_LOVINIT,
	USER_QUIT,
	USER_TOGGLE_PAUSE,
	USER_STEP,
};

int userInput()
{
	SDL_Event evt;
	int ret = USER_LOVINIT;
	while ( SDL_PollEvent (&evt) ) {
		// printf ("Event type: %d\n", evt.type);
		switch ( evt.type ) {
			case SDL_MOUSEBUTTONDOWN: {
				printf ("Mouse click!\n");
				ret = USER_QUIT;
				break;
			}
			case SDL_KEYUP: {
				switch ( evt.key.keysym.sym ) {
					case SDLK_SPACE:
						printf ("Space bar -> pause\n");
						ret = USER_TOGGLE_PAUSE;
						break;
					case SDLK_s:
						printf ("s -> step\n");
						ret = USER_STEP;
						break;
					case SDLK_q:
						printf ("q -> quit\n");
						ret = USER_QUIT;
						break;
				}
				break;
			}
		}
	}
	return ret;
}

//-----------------------------------------------------------------------------

void draw (context_t *ctx)
{
	SDL_SetRenderTarget (ctx->renderer, NULL);  // Render to window.
	SDL_SetRenderDrawColor (ctx->renderer, 0x00, 0x00, 0x00, 0x00);
	SDL_RenderClear (ctx->renderer);
	SDL_Rect dest;
	for (int i = 0; i < ctx->n; ++i) {
		const float r = ctx->diam[i] / 2;
		dest.x = ctx->x[i] - r;
		dest.y = ctx->y[i] - r;
		dest.w = 2 * r;
		dest.h = 2 * r;
		SDL_RenderCopy (ctx->renderer, ctx->textures[i], NULL, &dest);
	}
	SDL_RenderPresent (ctx->renderer);
}

//-----------------------------------------------------------------------------

// Calculate new velocities after collision given the (x, y) separation
// between two bodies and their velocities before collision.

void doCollision (
	float dx, float dy, float r,
	float *vxi, float *vyi, float *vxj, float *vyj)
{
}

void physics (context_t *ctx)
{
	// Update velocity of each body by applying the effects of
	// gravitational force and any collisions.
	for (int i = 0; i < ctx->n; ++i) {
		const float
			  xi = ctx->x[i]
			, yi = ctx->y[i]
			, di = ctx->diam[i]
			, mi = di * di                  // Mass in a 2-D world.
			;
		for (int j = i + 1; j < ctx->n; ++j) {
			const float
				  xj = ctx->x[j]
				, yj = ctx->y[j]
				, dj = ctx->diam[j]
				, mj = dj * dj
				, dx = xj - xi
				, dy = yj - yi
				, r2 = dx * dx + dy * dy
				, r = sqrtf (r2)
				,  force = G * mi * mj / r2
				, xforce = force / r * dx    // Resolve using
				, yforce = force / r * dy    // triangle similarity.
				;
			float
				  *vxi = &(ctx->vx[i])
				, *vyi = &(ctx->vy[i])
				, *vxj = &(ctx->vx[j])
				, *vyj = &(ctx->vy[j])
				;

			// Collision Handling

			const float minSep = (di + dj) / 2.0f;
			if ( r <= minSep ) {

				printf ("%d and %d colliding! r=%f minSep=%f\n",
				          i,     j,           r,   minSep);

				doCollision (dx, dy, r, vxi, vyi, vxj, vyj);
			}

			// Gravitational Impulse

			*vxi += xforce / mi;
			*vyi += yforce / mi;
			*vxj -= xforce / mj;
			*vyj -= yforce / mj;
		}
	}

	// Update position of each body using its new velocity.
	for (int i = 0; i < ctx->n; ++i) {
		ctx->x[i] += ctx->vx[i];
		ctx->y[i] += ctx->vy[i];
		// printf ("%6d: %8.2f, %8.2f\n", i, ctx->x[i], ctx->y[i]);
	}
}

//-----------------------------------------------------------------------------

void seeIfUserWantsSomething (context_t *ctx)
{
	switch (userInput()) {
		case USER_QUIT: {
#ifdef EMSCRIPTEN
			emscripten_cancel_main_loop();
#else
			cancelMainLoop();
#endif
			destroyTextures (ctx->n, ctx->textures);
			destroyContext (ctx);
			SDL_Quit();
			printf ("Bye\n");
			break;
		}
		case USER_TOGGLE_PAUSE: {
			if ( ctx->stepFrames == 0 )
				// Paused -> running.
				ctx->stepFrames = -1;
			else
				// Running or stepping -> paused.
				ctx->stepFrames = 0;
			break;
		}
		case USER_STEP: {
			++ctx->stepFrames;
			break;
		}
	}
}

//-----------------------------------------------------------------------------

void step (void *arg)
{
	context_t *ctx = arg;

	if ( ctx->stepFrames != 0 ) {
		// Not paused.

		draw (ctx);

		physics (ctx);

		if ( ctx->stepFrames > 0 ) {
			// Step N frames.
			--ctx->stepFrames;
		}
	} else  {
		// Paused.  Give up CPU for a while and then loop.
		SDL_Delay (10);
	}

	seeIfUserWantsSomething (ctx);
}

//-----------------------------------------------------------------------------

int main()
{
	printf ("Hey\n");

	SDL_Init (SDL_INIT_VIDEO);

	SDL_Window *window;
	SDL_Renderer *renderer;

	SDL_CreateWindowAndRenderer (900, 600, 0, &window, &renderer);

	const int N = 3;
	context_t context;
	initContext (&context, N);

	context.x   [0] = 100;
	context.y   [0] = 10;
	context.vx  [0] = 1.1f;
	context.vy  [0] = 0;
	context.diam[0] = 18;

	context.x   [1] = 800;
	context.y   [1] = 10;
	context.vx  [1] = 0.05f;
	context.vy  [1] = 0.7f;
	context.diam[1] = 24;

	context.x   [2] = 450;
	context.y   [2] = 300;
	context.vx  [2] = -0.4f;
	context.vy  [2] = 0.1f;
	context.diam[2] = 40;

	context.renderer = renderer;
	createTextures (&context);

#ifdef EMSCRIPTEN
	emscripten_set_main_loop_arg (step, &context, -1, /* don't continue = */ 1);
	printf ("We never get here.\n");
#else
	mainLoop (step, &context, 20);
#endif
}
