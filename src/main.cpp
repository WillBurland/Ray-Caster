#include <math.h>
#include <thread>

#include <SFML/Graphics.hpp>

#include "camera.cpp"
#include "colour.cpp"
#include "light.cpp"
#include "material.cpp"
#include "sphere.cpp"
#include "utility.cpp"
#include "vec3.cpp"

// threading global variables
int pixelsRendered = 0;
int availableThreads = std::thread::hardware_concurrency();
int totalThreads = std::thread::hardware_concurrency();

// window global variables
const int WINDOW_WIDTH = 512;
const int WINDOW_HEIGHT = 512;
const int BLOCK_SIZE_X = WINDOW_WIDTH / sqrt(totalThreads);
const int BLOCK_SIZE_Y = WINDOW_HEIGHT / sqrt(totalThreads);
const float GAMMA = 2.2f;

// pixel buffer
sf::Uint8 PIXELS[4 * WINDOW_WIDTH * WINDOW_HEIGHT];

// background colours
Colour TOP_BG_COL(173, 200, 255);
Colour BOT_BG_COL(107, 171, 255);

// list of all spheres in the scene
Sphere SPHERE_LIST[] = 
{
	Sphere(Vec3(-0.8f,  0.8f,  9.0f), 1.0f, Material(Colour(255, 128,  64), 0.3f, 1.0f, 1500.0f, 0.08f)), // orange
	Sphere(Vec3(-0.8f, -0.2f,  7.0f), 1.0f, Material(Colour( 64, 255, 128), 0.4f, 0.5f,  200.0f, 0.08f)), // green
	Sphere(Vec3(1.2f,  -0.5f,  7.7f), 1.0f, Material(Colour(128,  64, 255), 0.6f, 0.2f,   30.0f, 0.08f)), // purple
	Sphere(Vec3(1.2f,   1.5f, 10.0f), 1.0f, Material(Colour(255, 255, 255), 0.7f, 0.4f,   20.0f, 0.08f)), // white
};

// list of all lights in the scene
Light LIGHT_LIST[] = 
{
	Light(Vec3(-5.0f,  5.0f, 5.0f), Colour(255, 128, 128), 50.0f), // red
	Light(Vec3( 0.0f, 10.0f, 4.0f), Colour(128, 255, 128), 80.0f), // green
	Light(Vec3( 3.0f,  2.0f, 4.0f), Colour(128, 128, 255), 30.0f), // blue
	Light(Vec3(-3.0f,  3.0f, 9.0f), Colour(255, 255, 255),  8.0f)  // white
};

// sphere and light count
const int SPHERE_COUNT = sizeof(SPHERE_LIST) / sizeof(Sphere);
const int LIGHT_COUNT = sizeof(LIGHT_LIST) / sizeof(Light);

//camera 
Camera CAMERA(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), 90.0f);

int main()
{
	// objects used to display PIXELS[]
	sf::Image image;
	sf::Texture texture;
	sf::Sprite sprite;
	image.create(WINDOW_WIDTH, WINDOW_HEIGHT, PIXELS);
	texture.loadFromImage(image);
	sprite.setPosition(0.0f, 0.0f);
	sprite.setTexture(texture);

	// window creation
	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Window", sf::Style::Titlebar | sf::Style::Close);
	window.setFramerateLimit(60);

	// available threads
	availableThreads = totalThreads;
	if (totalThreads <= 0)
		totalThreads = 1;

	printf("Available threads: %d\n", totalThreads);
	printf("Block size: X: %i, Y: %i\n", BLOCK_SIZE_X, BLOCK_SIZE_Y);

	if (WINDOW_WIDTH % totalThreads != 0 || WINDOW_HEIGHT % totalThreads != 0)
	{
		printf("WARNING: Window size is not divisible by the number of threads, some pixels may not be rendered.\n");
	}

	// fps counter
	sf::Clock clock;
	float lastFpsList[5];
	float lastTime = 0.0f;

	while (window.isOpen())
	{
		// window event handling
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
		}

		// keyboard input
		float speed = 0.1f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A))
            CAMERA.pos.x -= speed;
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::D))
            CAMERA.pos.x += speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift))
            CAMERA.pos.y -= speed;
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
            CAMERA.pos.y += speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S))
            CAMERA.pos.z -= speed;
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::W))
            CAMERA.pos.z += speed;
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
			CAMERA.rot.x += speed * 0.25f;
		else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
			CAMERA.rot.x -= speed * 0.25f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
            CAMERA.rot.y -= speed * 0.25f;
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
            CAMERA.rot.y += speed * 0.25f;

		// render calculations
		pixelsRendered = 0;

		// set up threads
		std::vector<std::thread> threads(totalThreads);
		int* availableThreadArray = (int*)malloc(sizeof(int) * totalThreads);
		for (int i = 0; i < totalThreads; i++)
		{
			availableThreadArray[i] = 1;
		}

		// start threads rendering blocks of pixels
		int x = 0;
		int y = 0;
		int count = 0;
		// while there are still pixels to render
		while (count < (WINDOW_WIDTH * WINDOW_HEIGHT) / (BLOCK_SIZE_X * BLOCK_SIZE_Y))
		{
			// re-join threads that have finished
			for (int i = 0; i < (int)threads.size(); i++)
			{
				if (threads[i].joinable())
				{
					threads[i].join();
					availableThreadArray[i] = 1;
				}
			}

			// start threads that are available
			for (int i = 0; i < (int)threads.size(); i++)
			{
				if (availableThreadArray[i] == 1)
				{
					// start thread and set it to unavailable
					threads[i] = std::thread(RenderPixelBlock, x, y);

					availableThreadArray[i] = 0;

					// move to next block
					x += BLOCK_SIZE_X;
					if (x >= WINDOW_WIDTH)
					{
						x = 0;
						y += BLOCK_SIZE_Y;
					}

					count++;
				}
			}
		}

		// wait for all threads to finish
		bool allThreadsDone = false;
		while (!allThreadsDone)
		{
			allThreadsDone = true;
			for (int i = 0; i < (int)threads.size(); i++)
			{
				if (threads[i].joinable())
				{
					threads[i].join();
					availableThreadArray[i] = 1;
				}
			}
			for (int i = 0; i < (int)threads.size(); i++)
			{
				if (availableThreadArray[i] == 0)
				{
					allThreadsDone = false;
					break;
				}
			}
		}

		// render
		window.clear();
		image.create(WINDOW_WIDTH, WINDOW_HEIGHT, PIXELS);
		texture.loadFromImage(image);
		sprite.setTexture(texture);
		window.draw(sprite);
		window.display();

		// fps counter
		float fps = 1.0f / (clock.restart().asSeconds() - lastTime);
		float avgFps = 0.0f;
		for (int i = 0; i < 4; i++)
		{
			lastFpsList[i] = lastFpsList[i + 1];
			avgFps += lastFpsList[i];
		}
		avgFps /= 5.0f;
		lastFpsList[4] = fps;

		window.setTitle("FPS: " + std::to_string((int)avgFps));
		float currentTime = clock.restart().asSeconds();
		lastTime = currentTime;
		
	}
	return 0;
}