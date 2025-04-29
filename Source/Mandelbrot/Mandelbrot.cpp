#undef NVRHI_HAS_D3D11
#include "HydraEngine/Base.h"
#include <format>
#include "icon.h"

#if NVRHI_HAS_D3D12
#include "Embeded/dxil/Mandelbrot_Main.bin.h"
#endif

#if NVRHI_HAS_VULKAN
#include "Embeded/spirv/Mandelbrot_Main.bin.h"
#endif

import HE;
import std;
import Math;
import nvrhi;
import ImGui;
import simdjson;
import Utils;

using namespace HE;

class Mandelbrot : public Layer
{
public:
	struct Parms
	{
		Math::dvec2 offset = { 0.0, 0.0 };
		double zoom = 1.0;               
		double padding0;                 
		Math::vec4 colors[10];           
		int width = 1920;
		int height = 1080;
		int maxIterations = 100;         
		int padding1;                    
	};

	nvrhi::DeviceHandle device;
	nvrhi::CommandListHandle commandList;
	nvrhi::TextureHandle icon, close, min, max, res;
	nvrhi::BindingLayoutHandle bindingLayout;
	nvrhi::BindingSetHandle bindingSets;
	nvrhi::TextureHandle outputTexture;
	nvrhi::BufferHandle constantBufferHandle;
	nvrhi::BufferHandle agentsBuffer;
	nvrhi::ShaderHandle cs;
	nvrhi::ComputePipelineHandle computePipeline;

	Parms parms;
	bool useViewportSize = true;
	Math::dvec2 mousePos = { 0.0f,0.0f };
	double zoomInFactor = 1.003;
	double zoomOutFactor = .997;

	bool enableTitlebar = true;
	bool enableSettingWindow = true;
	bool enableViewPortWindow = true;

	std::string currentSetting;
	std::string recordingStateStr = "Record";
	std::string recordDir;

	bool recording = false;
	uint32_t frameIndex = 0;

	virtual void OnAttach() override
	{
		HE_PROFILE_FUNCTION();

		device = RHI::GetDevice();
		commandList = device->createCommandList();

		// Plugins
		{
			Plugins::LoadPluginsInDirectory("Plugins");
		}

		// icons
		{
			HE_PROFILE_SCOPE("Load Icons");

			commandList->open();

			icon = Utils::LoadTexture(Application::GetApplicationDesc().windowDesc.iconFilePath, device, commandList);
			close = Utils::LoadTexture(Buffer(g_icon_close, sizeof(g_icon_close)), device, commandList);
			min = Utils::LoadTexture(Buffer(g_icon_minimize, sizeof(g_icon_minimize)), device, commandList);
			max = Utils::LoadTexture(Buffer(g_icon_maximize, sizeof(g_icon_maximize)), device, commandList);
			res = Utils::LoadTexture(Buffer(g_icon_restore, sizeof(g_icon_restore)), device, commandList);

			commandList->close();
			device->executeCommandList(commandList);
		}

		// Shaders
		{
			HE_PROFILE_SCOPE("Load Shader");

			auto desc = nvrhi::ShaderDesc();
			desc.shaderType = nvrhi::ShaderType::Compute;

			desc.entryName = "Main";
			cs = RHI::CreateStaticShader(device, STATIC_SHADER(Mandelbrot_Main), nullptr, desc); 
			HE_ASSERT(cs);
		}

		// Settings
		{
			HE_PROFILE_SCOPE("Load Settings");

			Deserialize("Resources/Settings/Mandelbrot/test1.json");
			currentSetting = "test1";


			std::string layoutPath = "Resources/layout.ini";
			ImGui::GetIO().IniFilename = nullptr;
			ImGui::LoadIniSettingsFromDisk(layoutPath.c_str());
		}

		CreateResources();
	}


	virtual void OnUpdate(const HE::FrameInfo& info) override
	{
		// UI
		{
			HE_PROFILE_SCOPE_NC("UI", 0xFF0000FF);

			float dpi = ImGui::GetWindowDpiScale();
			float scale = ImGui::GetIO().FontGlobalScale * dpi;
			auto& style = ImGui::GetStyle();
			style.FrameRounding = Math::max(3.0f * scale, 1.0f);
			ImGui::ScopedStyle wb(ImGuiStyleVar_WindowBorderSize, 0.0f);

			ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID, ImGui::GetMainViewport(), ImGuiDockNodeFlags_::ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar);

			if (enableTitlebar)
			{
				bool customTitlebar = Application::GetApplicationDesc().windowDesc.customTitlebar;
				bool isIconClicked = Utils::BeginMainMenuBar(customTitlebar, icon.Get(), close.Get(), min.Get(), max.Get(), res.Get());

				if (ImGui::BeginMenu("Edit"))
				{
					if (ImGui::MenuItem("Restart", "Left Shift + ESC"))
						Application::Restart();

					if (ImGui::MenuItem("Exit", "ESC"))
						Application::Shutdown();

					if (ImGui::MenuItem("Full Screen", "F", Application::GetWindow().IsFullScreen()))
						Application::GetWindow().ToggleScreenState();

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Window"))
				{
					if (ImGui::MenuItem("TitleBar", "T", enableTitlebar))
						enableTitlebar = enableTitlebar ? false : true;

					if (ImGui::MenuItem("View Port", "Left Alt + 1", enableViewPortWindow))
						enableViewPortWindow = enableViewPortWindow ? false : true;

					if (ImGui::MenuItem("Setting", "Left Alt + 2", enableSettingWindow))
						enableSettingWindow = enableSettingWindow ? false : true;

					ImGui::EndMenu();
				}

				Utils::EndMainMenuBar();
			}

			if (enableViewPortWindow)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_WindowPadding, { 0.0f,0.0f });
				if (ImGui::Begin("View"))
				{
					Math::dvec2 size = Math::dvec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

					if (useViewportSize && (parms.width != size.x || parms.height != size.y))
					{
						parms.width = uint32_t(size.x);
						parms.height = uint32_t(size.y);
						Jops::SubmitToMainThread([this]() { CreateResources(); });
					}

					ImGui::Image(outputTexture.Get(), ImVec2{ (float)size.x, (float)size.y });

					ImGuiIO& io = ImGui::GetIO();
					bool isHovered = ImGui::IsItemHovered();
					Math::dvec2 windowPos = { ImGui::GetWindowPos().x, ImGui::GetWindowPos().y };
					Math::dvec2 mousePos = { ImGui::GetMousePos().x, ImGui::GetMousePos().y };

					auto localMousePos = ((mousePos - windowPos) / size) * Math::dvec2((float)parms.width, (float)parms.height);
					mousePos = { localMousePos.x, localMousePos.y };

					bool inc = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
					bool dec = ImGui::IsKeyDown(ImGuiKey_LeftShift);
					if (inc)
					{
						zoomInFactor += double(io.MouseWheel) * 0.001;
						Math::dvec2 halfSize(parms.width / 2.0, parms.height / 2.0);
						Math::dvec2 offset = mousePos - halfSize;
						parms.offset = parms.offset * zoomInFactor + offset * (zoomInFactor - 1.0);
						parms.zoom *= zoomInFactor;
					}
					else if (dec)
					{
						zoomOutFactor += double(io.MouseWheel) * 0.001;
						Math::dvec2 halfSize(parms.width / 2.0, parms.height / 2.0);
						Math::dvec2 offset = mousePos - halfSize;
						parms.offset = parms.offset * zoomOutFactor + offset * (zoomOutFactor - 1.0);
						parms.zoom *= zoomOutFactor;
					}
					else if ((isHovered && io.MouseWheel != 0.0))
					{
						double zf = (io.MouseWheel > 0) ? 1.1 : 0.9;
						Math::dvec2 halfSize(parms.width / 2.0, parms.height / 2.0);
						Math::dvec2 offset = mousePos - halfSize;
						parms.offset = parms.offset * zf + offset * (zf - 1.0);
						parms.zoom *= zf;
					}

					if (isHovered && ImGui::IsMouseDragging(ImGuiMouseButton_::ImGuiMouseButton_Middle))
					{
						glm::vec2 delta(io.MouseDelta.x, io.MouseDelta.y);
						parms.offset -= delta;
					}

					double x = 0, y = 0;
					double speed = ImGui::IsKeyDown(ImGuiKey_RightShift) ? 5.0 : 1;

					x += ImGui::IsKeyDown(ImGuiKey_LeftArrow) ? speed : 0;
					y += ImGui::IsKeyDown(ImGuiKey_UpArrow) ? speed : 0;
					x -= ImGui::IsKeyDown(ImGuiKey_RightArrow) ? speed : 0;
					y -= ImGui::IsKeyDown(ImGuiKey_DownArrow) ? speed : 0;

					if (x != 0 || y != 0)
					{
						parms.offset -= glm::vec2(x, y);
					}

					if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
					{
						glm::vec2 delta(io.MouseDelta.x, io.MouseDelta.y);
						parms.offset -= delta;
					}
				}
				ImGui::End();
				ImGui::PopStyleVar();
			}

			if (enableSettingWindow)
			{
				ImGui::ScopedStyle ss(ImGuiStyleVar_WindowPadding, ImVec2(4,4));
				ImGui::ScopedColor sc0(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
				ImGui::ScopedColor sc1(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
				ImGui::ScopedColor sc2(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
				if (ImGui::Begin("Setting", &enableSettingWindow))
				{
					auto cf = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

					ImGui::BeginChild("Appliction", {0,0}, cf);
					if (ImGui::CollapsingHeader("Appliction", ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen))
					{
						const auto& appStats = Application::GetStats();
						ImGui::Text("Graphics API : %s", nvrhi::utils::GraphicsAPIToString(RHI::GetDevice()->getGraphicsAPI()));
						ImGui::Text("FPS : %zd", appStats.FPS);
						ImGui::Text("CPUMainTime %f ms", appStats.CPUMainTime);
					}
					ImGui::EndChild();

					ImGui::BeginChild("Export", { 0,0 }, cf);
					if (ImGui::CollapsingHeader("Export", ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen))
					{
						if (ImGui::Button("Save", { ImGui::GetContentRegionAvail().x, 0 }))
						{
							auto file = FileDialog::SaveFile({ { "setting","json" } });
							Serialize(file);
						}

						if (ImGui::BeginCombo("Settings", currentSetting.c_str()))
						{
							for (auto& filePath : std::filesystem::directory_iterator("Resources/Settings/Mandelbrot"))
							{
								if (ImGui::Selectable(filePath.path().stem().string().c_str()))
								{
									currentSetting = filePath.path().stem().string();
									Deserialize(filePath.path());
									Jops::SubmitToMainThread([this]() { CreateResources(); });
								}
							}
							ImGui::EndCombo();
						}

						if (ImGui::Button(" ... "))
						{
							recordDir = FileDialog::SelectFolder().string();
						}

						ImGui::SameLine();

						if (recording)
						{
							ImGui::PushStyleColor(ImGuiCol_::ImGuiCol_Button, { 0.2f, 0.8f,0.3f, 0.9f });
							ImGui::PushStyleColor(ImGuiCol_::ImGuiCol_ButtonActive, { 0.2f, 0.8f,0.3f, 0.8f });
							ImGui::PushStyleColor(ImGuiCol_::ImGuiCol_ButtonHovered, { 0.2f, 0.8f,0.3f, 1.0f });
						}
						if (ImGui::Button(recordingStateStr.c_str(), { -1,0 }))
						{
							Jops::SubmitToMainThread([this]() {

								recording = recording ? false : true;
								recordingStateStr = recording ? "Stop" : "Record";

							});
						}
						if (recording) ImGui::PopStyleColor(3);
					}
					ImGui::EndChild();

					ImGui::BeginChild("Envairoment", { 0,0 }, cf);
					if (ImGui::CollapsingHeader("Envairoment", ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Checkbox("Use ViewPort Size", &useViewportSize);
						if (!useViewportSize)
						{
							if (ImGui::DragInt("Width", &parms.width, 1))
							{
								parms.width = parms.width < 8 ? 8 : parms.width;
								Jops::SubmitToMainThread([this]() { CreateResources(); });
							}

							if (ImGui::DragInt("Height", &parms.height, 1))
							{
								parms.height = parms.height < 8 ? 8 : parms.height;
								Jops::SubmitToMainThread([this]() { CreateResources(); });
							}
						}
						else
						{
							ImGui::Text("Width  : %zd", parms.width);
							ImGui::Text("Height : %zd", parms.height);
						}

						ImGui::DragInt("maxIterations", &parms.maxIterations, 1);
						
						for (int i = 0; i < std::size(parms.colors); i++)
						{
							ImGui::ColorEdit3(std::to_string(i).c_str(), &parms.colors[i].x);
						}
					}
					ImGui::EndChild();

				}
				ImGui::End();
			}
		}

		{
			if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::Escape))
			{
				Application::Restart();
				return;
			}

			if (Input::IsKeyPressed(Key::Escape))
				Application::Shutdown();

			if (Input::IsKeyReleased(Key::F))
				Application::GetWindow().ToggleScreenState();

			if (Input::IsKeyPressed(Key::T))
				enableTitlebar = enableTitlebar ? false : true;

			if (Input::IsKeyDown(Key::LeftAlt) && Input::IsKeyPressed(Key::D1))
				enableViewPortWindow = enableViewPortWindow ? false : true;

			if (Input::IsKeyDown(Key::LeftAlt) && Input::IsKeyPressed(Key::D2))
				enableSettingWindow = enableSettingWindow ? false : true;

			if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyReleased(Key::M))
			{
				if (!Application::GetWindow().IsMaximize())
					Application::GetWindow().MaximizeWindow();
				else
					Application::GetWindow().RestoreWindow();
			}

			if (Input::IsKeyPressed(Key::H))
			{
				if (Input::GetCursorMode() == Cursor::Mode::Disabled)
					Input::SetCursorMode(Cursor::Mode::Normal);
				else
					Input::SetCursorMode(Cursor::Mode::Disabled);
			}
		}

		{
			HE_PROFILE_SCOPE("dispatch");

			commandList->writeBuffer(constantBufferHandle, &parms, sizeof(parms));
			commandList->setComputeState({ computePipeline ,{ bindingSets } });
			commandList->dispatch(int(parms.width / 8), int(parms.height / 8));
		}
	}

	virtual void OnBegin(const HE::FrameInfo& info) override
	{
		commandList->open();
		nvrhi::utils::ClearColorAttachment(commandList, info.fb, 0, nvrhi::Color(0.0f));
	}

	virtual void OnEnd(const HE::FrameInfo& info) override
	{
		commandList->close();
		device->executeCommandList(commandList);
		device->waitForIdle();

		if (recording)
		{
			Utils::Record(device, commandList, outputTexture, frameIndex, recordDir);
		}
	}

	void CreateResources()
	{
		HE_PROFILE_FUNCTION();

		outputTexture.Reset();
		bindingLayout.Reset();
		bindingSets.Reset();

		// output texture
		{
			nvrhi::TextureDesc desc;
			desc.width = parms.width;
			desc.height = parms.height;
			desc.format = nvrhi::Format::RGBA8_UNORM;
			desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
			desc.debugName = "outputColor";
			desc.isUAV = true;
			outputTexture = device->createTexture(desc);
		}

		// Binding
		{
			constantBufferHandle = device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(Parms), "Parms", sizeof(Parms)));

			nvrhi::BindingSetDesc bindingSetDesc;
			bindingSetDesc.bindings = {
				nvrhi::BindingSetItem::ConstantBuffer(0, constantBufferHandle),
				nvrhi::BindingSetItem::Texture_UAV(0,outputTexture),
			};

			bool res = nvrhi::utils::CreateBindingSetAndLayout(device, nvrhi::ShaderType::Compute, 0, bindingSetDesc, bindingLayout, bindingSets);
			HE_ASSERT(res);
		}

		// Compute Pipelines
		{
			computePipeline = device->createComputePipeline({ cs,{ bindingLayout } }); HE_ASSERT(computePipeline);
		}
	}

	void Serialize(const std::filesystem::path& path) const
	{
		std::ofstream file(path);
		if (!file.is_open())
		{
			HE_ERROR("Error: Unable to open file for writing: {}", path.string());
			return;
		}

		std::ostringstream oss;
		oss << "{"
			<< "\n\t\"width\":" << parms.width << ","
			<< "\n\t\"height\":" << parms.height << ","
			<< "\n\t\"maxIterations\":" << parms.maxIterations << ","
			<< "\n\t\"offset\":" << parms.offset << ","
			<< "\n\t\"zoom\":" << parms.zoom << ",";
		oss << "\n\t\"colors\": [";
		for (int i = 0; i < std::size(parms.colors); i++)
		{
			oss << "\n\t\t[" << parms.colors[i].x << "," << parms.colors[i].y << "," << parms.colors[i].z << "," << parms.colors[i].w << "]";
			if (i != std::size(parms.colors) - 1)  oss << ",";
		}
		oss << "\n\t]";
		oss << "\n}";

		file << oss.str();
		file.close();
	}

	void Deserialize(const std::filesystem::path& path)
	{
		if (!std::filesystem::exists(path))
			return;

		static simdjson::dom::parser parser;
		auto doc = parser.load(path.string());

		parms.width = (int)doc["width"].get_int64();
		parms.height = (int)doc["height"].get_int64();
		parms.maxIterations = (int)doc["maxIterations"].get_int64();

		parms.zoom = doc["zoom"].get_double();
		auto offset = doc["offset"].get_array();
		parms.offset.x = offset.at(0).get_double();
		parms.offset.y = offset.at(1).get_double();

		auto colors = doc["colors"].get_array();
		for (int i = 0; i < std::size(parms.colors); i++)
		{
			parms.colors[i].x = (float)colors.at(i).at(0).get_double();
			parms.colors[i].y = (float)colors.at(i).at(1).get_double();
			parms.colors[i].z = (float)colors.at(i).at(2).get_double();
			parms.colors[i].w = (float)colors.at(i).at(3).get_double();
		}
	}
};


HE::ApplicationContext* HE::CreateApplication(ApplicationCommandLineArgs args)
{
	ApplicationDesc desc;

	desc.deviceDesc.api = {
		nvrhi::GraphicsAPI::D3D12,
		nvrhi::GraphicsAPI::VULKAN
	};

	desc.windowDesc.title = "Mandelbrot";
	desc.windowDesc.iconFilePath = "Resources/Icons/Hydra.png";
	desc.windowDesc.customTitlebar = true;
	desc.windowDesc.maximized = true;
	desc.windowDesc.minWidth = 960;
	desc.windowDesc.minHeight = 540;

	ApplicationContext* ctx = new ApplicationContext(desc);
	Application::PushLayer(new Mandelbrot());

	return ctx;
}

#include "HydraEngine/EntryPoint.h"