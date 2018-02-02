// Compile for Emscripten with
//   emcc  -s USE_SDL=2 -o main.{html,c}
// Compile for native with
//   cc -o main{,.c} -I/usr/include/SDL2 -lSDL2 -lm

#include <SDL.h>
#include <SDL_events.h>
#include <SDL_render.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#else
#include <unistd.h>               // usleep
#endif

#include <stdio.h>

//-----------------------------------------------------------------------------

const float G = 0.0005f;

//-----------------------------------------------------------------------------

typedef struct {
	int n;  // Length of arrays.
	float *x,  *y;
	float *vx, *vy;
	float *diam;
	SDL_Renderer *renderer;
	SDL_Texture **textures;
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
	SDL_Rect r;
	r.x = 0; r.y = diam / 3; r.w = diam; r.h = diam / 3;
	SDL_RenderFillRect (renderer, &r);
	r.x = diam / 3; r.y = 0; r.w = diam / 3; r.h = diam;
	SDL_RenderFillRect (renderer, &r);
}

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
	for ( continueMainLoop = 1; continueMainLoop; usleep (1000 * sleepMs) )
		(*fn)(context);
}

#endif

//-----------------------------------------------------------------------------

int userQuit()
{
	SDL_Event evt;
	while ( SDL_PollEvent (&evt) ) {
		printf ("Event type: %d\n", evt.type);
		if ( evt.type == SDL_MOUSEBUTTONDOWN ) {
			printf ("Mouse click!\n");
			return 1;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------

void step (void *arg)
{
	context_t *ctx = arg;
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

	for (int i = 0; i < ctx->n; ++i) {
		const int xi = ctx->x[i];
		const int yi = ctx->y[i];
		const int mi = ctx->diam[i] * ctx->diam[i];
		for (int j = 0; j < ctx->n; ++j) {
			if ( j == i ) continue;
			const int xj = ctx->x[j];
			const int yj = ctx->y[j];
			const int mj = ctx->diam[j] * ctx->diam[j];
			const int dx = xj - xi;
			const int dy = yj - yi;
			const int r2 = dx * dx + dy * dy;
			const float  force = G * mi * mj / r2;
			const float      r = sqrtf (r2);
			const float xforce = force / r * dx;
			const float yforce = force / r * dy;
			ctx->vx[i] += xforce;
			ctx->vy[i] += yforce;
		}
		ctx->x[i] += ctx->vx[i];
		ctx->y[i] += ctx->vy[i];
		printf ("%6d: %8.2f, %8.2f\n", i, ctx->x[i], ctx->y[i]);

		if ( userQuit() || ctx->x[i] < -100 || ctx->y[i] < -100 ) {
#ifdef EMSCRIPTEN
			emscripten_cancel_main_loop();
#else
			cancelMainLoop();
#endif
			destroyTextures (ctx->n, ctx->textures);
			destroyContext (ctx);
			SDL_Quit();
			printf ("Bye\n");
			return;
		}
	}
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

	context.x   [0] = 10;
	context.y   [0] = 10;
	context.vx  [0] = 1.0f;
	context.vy  [0] = 0;
	context.diam[0] = 18;

	context.x   [1] = 800;
	context.y   [1] = 10;
	context.vx  [1] = 0.05f;
	context.vy  [1] = 0.4f;
	context.diam[1] = 24;

	context.x   [2] = 400;
	context.y   [2] = 400;
	context.vx  [2] = -1.0f;
	context.vy  [2] = 0.3f;
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
