#pragma once
#include <cassert>

#pragma warning(push)
#pragma warning(disable : 26819) // disable the fallthrough between switch labels warning
	#include <SDL_keyboard.h>
	#include <SDL_mouse.h>
#pragma warning(pop)

#include "MathHelpers.h"
#include "Vector3.h"
#include "Timer.h"
#include "Matrix.h"

namespace mau
{
	struct Camera final
	{
		Camera() = default;

		Camera(const Vector3& _origin, float _fovAngle) :
			origin{ _origin },
			fovAngle{ _fovAngle }
		{ }

		Vector3 origin{};
		float fovAngle{ 90.f };
		float fov{ tanf((fovAngle * TO_RADIANS) / 2.f) };
		float aspectRatio{};

		Vector3 forward{ Vector3::UnitZ };
		Vector3 up{ Vector3::UnitY };
		Vector3 right{ Vector3::UnitX };

		float totalPitch{};
		float totalYaw{};

		float movementSpeed{ 10.f };
		float sprintSpeed{ 3.f };
		float rotationSpeed{ 5.f };

		float nearPlane{ 0.1f };
		float farPlane{ 100.f };

		Matrix invViewMatrix{};
		Matrix viewMatrix{};
		Matrix projectionMatrix{};

		void Initialize(float _fovAngle = 90.f, Vector3 const& _origin = { 0.f,0.f,0.f }, float _aspectRatio = 19.f / 6.f)
		{
			fovAngle = _fovAngle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);

			origin = _origin;

			aspectRatio = _aspectRatio;

			CalculateViewMatrix();
			CalculateProjectionMatrix();
		}

#pragma region CameraSettings
		void UpdateCameraSettings(float _fovAngle = 90.f, Vector3 const& _origin = { 0.f, 0.f, 0.f }, float _aspectRatio = 19.f / 6.f)
		{
			fovAngle = _fovAngle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);

			origin = _origin;

			aspectRatio = _aspectRatio;

			CalculateProjectionMatrix();
		}

		void UpdateFov(float angle)
		{
			fovAngle = angle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);
			CalculateProjectionMatrix();
		}
		void UpdateAspectRatio(float ratio)
		{
			aspectRatio = ratio;
			CalculateProjectionMatrix();
		}
		void UpdateOrigin(Vector3 const& pos)
		{
			origin = pos;
		}
#pragma endregion

		void CalculateViewMatrix()
		{
			right = Vector3::Cross(Vector3::UnitY, forward);
			up = Vector3::Cross(forward, right);

			invViewMatrix = { right, up, forward, origin };
			viewMatrix = Matrix::Inverse(invViewMatrix);
		}

		void CalculateProjectionMatrix()
		{
			projectionMatrix = Matrix::CreatePerspectiveFovLH(fov, aspectRatio, nearPlane, farPlane);
		}

		void Update(const Timer* pTimer)
		{
			float const deltaTime{ pTimer->GetElapsed() };

			//Keyboard Input
			uint8_t const* pKeyboardState{ SDL_GetKeyboardState(nullptr) };
			assert(pKeyboardState);
			bool sprinting{ false };
			Vector3 movementDir{ };

			if (pKeyboardState[SDL_SCANCODE_W] || pKeyboardState[SDL_SCANCODE_UP])
				movementDir += forward;
			if (pKeyboardState[SDL_SCANCODE_S] || pKeyboardState[SDL_SCANCODE_DOWN])
				movementDir -= forward;
			if (pKeyboardState[SDL_SCANCODE_A] || pKeyboardState[SDL_SCANCODE_LEFT])
				movementDir -= right;
			if (pKeyboardState[SDL_SCANCODE_D] || pKeyboardState[SDL_SCANCODE_RIGHT])
				movementDir += right;
			if (pKeyboardState[SDL_SCANCODE_LSHIFT])
				sprinting = true;

			auto adjustedMovementSpeed = movementSpeed;
			if (sprinting)
			{
				adjustedMovementSpeed *= sprintSpeed;
			}

			if (! (movementDir.x == 0.f && movementDir.y == 0.f && movementDir.z == 0.f) )
			{
				movementDir.Normalize();
				origin += movementDir * deltaTime * adjustedMovementSpeed;
			}

			//Mouse Input
			int mouseX{};
			int mouseY{};
			uint32_t const mouseState{ SDL_GetRelativeMouseState(&mouseX, &mouseY) };

			// LMB and RMB move up / down
			if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT) and mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT))
			{
				origin += up * (-mouseY * adjustedMovementSpeed * 10.f * deltaTime);
			}
			else
			{
				// LMB - move forward / backward and rotate yaw 
				if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT))
				{
					origin += forward * (-mouseY * adjustedMovementSpeed * 10.f * deltaTime);
					forward = Matrix::CreateRotationY(mouseX * rotationSpeed * deltaTime).TransformVector(forward);
				}

				// RMB - rotate pitch and yaw
				if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT))
				{

					forward = Matrix::CreateRotationY(mouseX * rotationSpeed * deltaTime).TransformVector(forward);
					forward = Matrix::CreateRotationX(-mouseY * rotationSpeed * deltaTime).TransformVector(forward);
				}
			}

			//Update Matrices
			CalculateViewMatrix();
			CalculateProjectionMatrix();
		}
	};
}