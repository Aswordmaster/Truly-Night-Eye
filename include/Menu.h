#pragma once

namespace night_eye::menu
{
	void Initialize(HWND window, ID3D11Device* device, ID3D11DeviceContext* context);
	void PollInput();
	void Render();
	bool IsOpen();
	void Close();
}
