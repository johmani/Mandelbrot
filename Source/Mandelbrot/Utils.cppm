module;

#include "HydraEngine/Base.h"

export module Utils;

import HE;
import std;
import Math;
import nvrhi;
import ImGui;

export namespace Utils {

	nvrhi::TextureHandle LoadTexture(const std::filesystem::path filePath, nvrhi::IDevice* device, nvrhi::ICommandList* commandList)
	{
		HE::Image image(filePath);
		nvrhi::TextureDesc desc;
		desc.width = image.GetWidth();
		desc.height = image.GetHeight();
		desc.format = nvrhi::Format::RGBA8_UNORM;
		desc.debugName = filePath.string();

		auto texture = device->createTexture(desc);
		commandList->beginTrackingTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
		commandList->writeTexture(texture, 0, 0, image.GetData(), desc.width * 4);
		commandList->setPermanentTextureState(texture, nvrhi::ResourceStates::ShaderResource);
		commandList->commitBarriers();

		return texture;
	}

	nvrhi::TextureHandle LoadTexture(HE::Buffer buffer, nvrhi::IDevice* device, nvrhi::ICommandList* commandList)
	{
		HE::Image image(buffer);

		nvrhi::TextureDesc desc;
		desc.width = image.GetWidth();
		desc.height = image.GetHeight();
		desc.format = nvrhi::Format::RGBA8_UNORM;
		desc.debugName = "random";

		nvrhi::TextureHandle texture = device->createTexture(desc);
		commandList->beginTrackingTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
		commandList->writeTexture(texture, 0, 0, image.GetData(), desc.width * 4);
		commandList->setPermanentTextureState(texture, nvrhi::ResourceStates::ShaderResource);
		commandList->commitBarriers();
	
		return texture;
	}


	bool BeginMainMenuBar(bool customTitlebar, ImTextureRef icon, ImTextureRef close, ImTextureRef min, ImTextureRef max, ImTextureRef res)
	{
		if (customTitlebar)
		{
			float dpi = ImGui::GetWindowDpiScale();
			float scale = ImGui::GetIO().FontGlobalScale * dpi;
			ImGuiStyle& style = ImGui::GetStyle();
			static float yframePadding = 8.0f * dpi;
			bool isMaximized = HE::Application::GetWindow().IsMaximize();
			auto& title = HE::Application::GetApplicationDesc().windowDesc.title;
			bool isIconClicked = false;

			{
				ImGui::ScopedStyle fbs(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::ScopedStyle wbs(ImGuiStyleVar_WindowBorderSize, 0.0f);
				ImGui::ScopedStyle wr(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 6.0f * dpi));
				ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2{ 0, yframePadding });
				ImGui::ScopedColor scWindowBg(ImGuiCol_WindowBg, ImVec4{ 1.0f, 0.0f, 0.0f, 0.0f });
				ImGui::ScopedColor scMenuBarBg(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 1.0f, 0.0f, 0.0f });
				ImGui::BeginMainMenuBar();
			}

			const ImVec2 windowPadding = style.WindowPadding;
			const ImVec2 titlebarMin = ImGui::GetCursorScreenPos() - ImVec2(windowPadding.x, 0);
			const ImVec2 titlebarMax = ImGui::GetCursorScreenPos() + ImGui::GetWindowSize() - ImVec2(windowPadding.x, 0);
			auto* fgDrawList = ImGui::GetForegroundDrawList();
			auto* bgDrawList = ImGui::GetBackgroundDrawList();

			ImGui::SetCursorPos(titlebarMin);

			//fgDrawList->AddRect(titlebarMin, titlebarMax, ImColor32(255, 0, 0, 255));
			//fgDrawList->AddRect(
			//	ImVec2{ titlebarMax.x / 2 - ImGui::CalcTextSize(title.c_str()).x, titlebarMin.y },
			//	ImVec2{ titlebarMax.x / 2 + ImGui::CalcTextSize(title.c_str()).x, titlebarMax.y },
			//	ImColor32(255, 0, 0, 255)
			//);

			bgDrawList->AddRectFilledMultiColor(
				titlebarMin,
				titlebarMax,
				ImColor32(50, 60, 80, 255),
				ImColor32(50, 50, 50, 255),
				ImColor32(50, 50, 50, 255),
				ImColor32(50, 50, 50, 255)
			);

			bgDrawList->AddRectFilledMultiColor(
				titlebarMin,
				ImGui::GetCursorScreenPos() + ImGui::GetWindowSize() - ImVec2(ImGui::GetWindowSize().x * 3 / 4, 0),
				ImColor32(50, 90, 110, 255),
				ImColor32(50, 50, 50, 0),
				ImColor32(50, 50, 50, 0),
				ImColor32(50, 90, 110, 255)
			);


			if (!ImGui::IsAnyItemHovered() && !HE::Application::GetWindow().IsFullScreen() && ImGui::IsWindowHovered())
				HE::Application::GetWindow().SetTitleBarState(true);
			else
				HE::Application::GetWindow().SetTitleBarState(false);

			float ySpace = ImGui::GetContentRegionAvail().y;

			// Text
			{
				ImGui::ScopedFont sf(1, 20);
				ImGui::ScopedStyle fr(ImGuiStyleVar_FramePadding, ImVec2{ 8.0f , 8.0f });

				ImVec2 cursorPos = ImGui::GetCursorPos();

				float size = ImGui::CalcTextSize(title.c_str()).x;
				float avail = ImGui::GetContentRegionAvail().x;

				float off = (avail - size) * 0.5f;
				if (off > 0.0f)
					ImGui::ShiftCursorX(off);

				//ImGui::ShiftCursorY(-2 * scale);
				ImGui::TextUnformatted(title.c_str());
				//ImGui::ShiftCursorY(2 * scale);

				ImGui::SetCursorPos(cursorPos);
			}

			// icon
			{
				ImVec2 cursorPos = ImGui::GetCursorPos();

				ImGui::ScopedColor sc0(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
				ImGui::ScopedColor sc1(ImGuiCol_ButtonActive, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
				ImGui::ScopedColor sc2(ImGuiCol_ButtonHovered, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

				if (ImGui::ImageButton("icon", icon, { ySpace, ySpace }))
				{
					isIconClicked = true;
				}

				ImGui::SetCursorPos(cursorPos);
				ImGui::SameLine();

			}

			// buttons
			{
				ImGui::ScopedStyle fr(ImGuiStyleVar_FrameRounding, 0.0f);
				ImGui::ScopedStyle is(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f));
				ImGui::ScopedStyle iis(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0.0f));

				ImVec2 cursorPos = ImGui::GetCursorPos();
				float size = ySpace * 3 + style.FramePadding.x * 6.0f;
				float avail = ImGui::GetContentRegionAvail().x;

				float off = (avail - size);
				if (off > 0.0f)
					ImGui::ShiftCursorX(off);

				{
					ImGui::ScopedColor sc0(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
					if (ImGui::ImageButton("min", min, { ySpace, ySpace })) HE::Application::GetWindow().MinimizeWindow();

					ImGui::SameLine();
					if (ImGui::ImageButton("max_res", isMaximized ? res : max, { ySpace, ySpace }))
						isMaximized ? HE::Application::GetWindow().RestoreWindow() : HE::Application::GetWindow().MaximizeWindow();
				}

				{
					ImGui::SameLine();
					ImGui::ScopedColor sc0(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
					ImGui::ScopedColor sc1(ImGuiCol_ButtonActive, ImVec4{ 1.0f, 0.3f, 0.2f, 1.0f });
					ImGui::ScopedColor sc2(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.3f, 0.2f, 1.0f });
					if (ImGui::ImageButton("close", close, { ySpace, ySpace })) HE::Application::Shutdown();
				}

				ImGui::SetCursorPos(cursorPos);
			}

			return isIconClicked;
		}
		else
		{
			ImGui::BeginMainMenuBar();
		}

		return false;
	}

	void EndMainMenuBar()
	{
		ImGui::EndMainMenuBar();
	}

	void Record(
		nvrhi::IDevice* device,
		nvrhi::ICommandList* commandList,
		nvrhi::ITexture* texture,
		uint32_t& frameIndex,
		const std::string& recordDir
	)
	{
		static std::mutex captureMutex;
		static std::condition_variable captureCV;
		static std::atomic<uint32_t> pendingFrameCount;
		int currentFrame = frameIndex++;

		{
			std::unique_lock<std::mutex> lock(captureMutex);
			captureCV.wait(lock, [] { return pendingFrameCount.load() < HE::Application::GetApplicationDesc().workersNumber * 2; });
			++pendingFrameCount;
		}

		auto fileName = std::format("{}\\{}.png", recordDir, std::to_string(currentFrame));

		auto textureState = nvrhi::ResourceStates::UnorderedAccess;
		nvrhi::TextureDesc desc = texture->getDesc();

		commandList->open();

		if (textureState != nvrhi::ResourceStates::Unknown)
		{
			commandList->beginTrackingTextureState(texture, nvrhi::TextureSubresourceSet(0, 1, 0, 1), textureState);
		}

		nvrhi::StagingTextureHandle stagingTexture = device->createStagingTexture(desc, nvrhi::CpuAccessMode::Read);
		HE_VERIFY(stagingTexture);

		commandList->copyTexture(stagingTexture, nvrhi::TextureSlice(), texture, nvrhi::TextureSlice());

		if (textureState != nvrhi::ResourceStates::Unknown)
		{
			commandList->setTextureState(texture, nvrhi::TextureSubresourceSet(0, 1, 0, 1), textureState);
			commandList->commitBarriers();
		}

		commandList->close();
		device->executeCommandList(commandList);

		HE::Jops::SubmitTask([device, fileName, desc, stagingTexture](){

			size_t rowPitch = 0;
			void* pData = device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch);
			HE_VERIFY(pData);

			std::vector<uint32_t> pixelData;
			void* dataPtr = pData;

			if (rowPitch != desc.width * 4)
			{
				pixelData.resize(desc.width * desc.height);
				for (uint32_t row = 0; row < desc.height; row++)
				{
					std::memcpy(
						pixelData.data() + row * desc.width,
						static_cast<char*>(pData) + row * rowPitch,
						desc.width * sizeof(uint32_t)
					);
				}
				dataPtr = pixelData.data();
			}

			bool writeSuccess = HE::Image::SaveAsPNG(fileName, desc.width, desc.height, 4, dataPtr, desc.width * 4) != 0;

			device->unmapStagingTexture(stagingTexture);

			// Mark frame as finished
			{
				std::lock_guard<std::mutex> lock(captureMutex);
				--pendingFrameCount;
				captureCV.notify_one();
			}
		});
	}
}