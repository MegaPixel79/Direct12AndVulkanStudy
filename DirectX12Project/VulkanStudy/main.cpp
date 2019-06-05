#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm/vec4.hpp>


#include <glm/glm/mat4x4.hpp>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>


const int kMAX_FRAMES_IN_FLIGHT = 2;


static const std::string red("\033[0;31m");
static const std::string green("\033[1;32m");
static const std::string yellow("\033[1;33m");
static const std::string cyan("\033[0;36m");
static const std::string magenta("\033[0;35m");
static const std::string reset("\033[0m");


static std::vector<char> ReadFile(const std::string& FileName)
{
	//Open a binary file and put the file cursor at the end of the file itself. 
	std::ifstream File(FileName,std::ios::ate | std::ios::binary);

	//If we failed in opening the file, we assert
	if (!File.is_open()) 
	{
		 throw std::runtime_error("Failed to open file!");	
	}

	//Get the file size by asking the cursor position in bytes
	size_t FileSize = (size_t)File.tellg();
	//Allocate a byte buffer
	std::vector<char> Buffer(FileSize);

	//Go the beginning of the file
	File.seekg(0);
	//Read FileSize bytes and store them in the previously allocated buffer
	File.read(Buffer.data(),FileSize);

	//Done, close the file
	File.close();

	//Return the buffer
	return Buffer;
}



VkResult CreateDebugUtilsMessengerEXT(VkInstance Instance, 
									  const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
									  const VkAllocationCallbacks* pAllocator, 
	                                  VkDebugUtilsMessengerEXT* pCallback ) 
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Instance,"vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) 
	{
		 return func(Instance, pCreateInfo, pAllocator,pCallback);
	}
	else
	{
		 return VK_ERROR_EXTENSION_NOT_PRESENT;	
	}
}


void DestroyDebugUtilsMessengerEXT( VkInstance Instance,
									VkDebugUtilsMessengerEXT Callback, 
							  const VkAllocationCallbacks* pAllocator ) 
{
	 auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Instance,"vkDestroyDebugUtilsMessengerEXT");
	 if (func != nullptr)
	 {
		 func(Instance, Callback, pAllocator);
	 }
}

class MyApplication
{
public:

	MyApplication() = default;

	~MyApplication() = default;

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR mCapabilities;   //number of images in the queue, images width/height 
		std::vector<VkSurfaceFormatKHR> mFormats; //Pixel format, color space
		std::vector<VkPresentModeKHR> mPresentModes; //the variouis presentation modes 		
	};

	struct QueueFamilyIndices
	{
		int32_t mGraphicsFamily = -1;

		int32_t mPresentFamily = -1;

		bool IsComplete()
		{
			return mGraphicsFamily >= 0 && mPresentFamily >= 0;
		}
	};

	static constexpr int32_t kScreenWidth = 800;
	static constexpr int32_t kScreenHeight = 600;

	const std::vector<const char*> ValidationLayers = { "VK_LAYER_LUNARG_standard_validation" };

	//Device extensions (just swap chain for now)
	const std::vector<const char*> DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
	const bool kEnableValidationLayers = false;
#else
	const bool kEnableValidationLayers = true;
#endif

	void Run()
	{
		InitWindow();
		InitVulkan();
		MainLoop();
		CleanUp();
	}

private:

	bool CheckValidationLayerSupport()
	{
		uint32_t LayerCount;
		vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);
		std::vector<VkLayerProperties> AvailableLayers(LayerCount);
		vkEnumerateInstanceLayerProperties(&LayerCount, AvailableLayers.data());
		for (const char* LayerName : ValidationLayers)
		{
			bool LayerFound = false;

			for (const auto& LayerProperties : AvailableLayers)
			{
				if (strcmp(LayerName, LayerProperties.layerName) == 0)
				{
					LayerFound = true;
					break;
				}
			}
			if (!LayerFound)
			{
				return false;
			}
		}
		return true;
	}

	std::vector<const char*> GetRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = nullptr;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> Extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (kEnableValidationLayers)
		{
			Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		return Extensions;
	}

	void InitWindow()
	{
		//Init GLFW
		glfwInit();

		//Disable the OpenGL context creation
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		//Disable the window resizing capability
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		//Create a window
		mWindow = glfwCreateWindow(kScreenWidth, kScreenHeight, "Vulkan", nullptr, nullptr);
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT MessageType,
		const VkDebugUtilsMessengerCallbackDataEXT* CallbackData,
		void* pUserData)
	{
		std::cerr << "Validation Layer: " << CallbackData->pMessage << std::endl;

		if (MessageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			// Message is important enough to show
		}

		return VK_FALSE;
	}


	void SetupDebugCallback()
	{
		if (!kEnableValidationLayers)
		{
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT CreateInfo = {};
		CreateInfo.sType =
			VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		CreateInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		CreateInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		CreateInfo.pfnUserCallback = DebugCallback;
		CreateInfo.pUserData = nullptr; // Optional

		if (CreateDebugUtilsMessengerEXT(mVkInstance, &CreateInfo, nullptr, &mCallback) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to set up debug callback!");
		}

	}

	void CreateVulkanInstance()
	{
		//Check whether the requested validation layer are available or not
		if (kEnableValidationLayers && !CheckValidationLayerSupport())
		{
			throw std::runtime_error("Validation Layers requested, but not available !");
		}

		//A struct that will hold application related info
		VkApplicationInfo AppInfo = {};
		AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		AppInfo.pApplicationName = "Hello World from Vulkan !";
		AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		AppInfo.pEngineName = "No Engine";
		AppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		AppInfo.apiVersion = VK_API_VERSION_1_0;

		//A struct that will hold info used to create the vulkan instance based on the application info struct and the supported extensions
		VkInstanceCreateInfo CreateInfo = {};
		CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		CreateInfo.pApplicationInfo = &AppInfo;

		//Ask for required extensions checking for the related validation layers as well. Check GetRequiredExtensions() implementation		
		auto ReqExtensions = GetRequiredExtensions();
		CreateInfo.enabledExtensionCount = static_cast<uint32_t>(ReqExtensions.size());
		CreateInfo.ppEnabledExtensionNames = ReqExtensions.data();


		//Check if validation layers are enabled and pass the relevant info to instrument the instance creation struct 
		if (kEnableValidationLayers)
		{
			CreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
			CreateInfo.ppEnabledLayerNames = ValidationLayers.data();
		}
		else
		{
			CreateInfo.enabledLayerCount = 0;
		}

		//Let's enumerate and display the supported extensions
		uint32_t ExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);
		std::vector<VkExtensionProperties> Extensions(ExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, Extensions.data());
		std::cout << yellow.c_str() << "Available extensions:" << reset.c_str() << std::endl;
		for (const auto& Ext : Extensions)
		{
			std::cout << "\t" << "- " << Ext.extensionName << std::endl;
		}


		//Check to see whether the returned extensions from glfwGetRequiredInstanceExtensions are contained in the total enumerated extensions
		uint32_t glfwExtensionCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::cout << yellow.c_str() << "\nExtensions returned from " << cyan.c_str() << "glfwGetRequiredInstanceExtensions(uint32_t* count) " << yellow.c_str() << "present in the enumerated list:" << reset.c_str() << std::endl;
		uint32_t RequiredExtCount = 0;
		for (uint32_t i = 0; i < glfwExtensionCount; ++i)
		{
			for (const auto& Ext : Extensions)
			{
				if (glfwExtensions && strcmp(Ext.extensionName, glfwExtensions[i]) == 0)
				{
					std::cout << "\t" << green.c_str() << "- " << Ext.extensionName << reset.c_str() << std::endl;
					++RequiredExtCount;
				}
			}
		}
		if (RequiredExtCount == glfwExtensionCount)
		{
			std::cout << "Success !!! All required extensions are supported !" << std::endl;
		}
		else
		{
			std::cout << "None or only some extensions are supported !" << std::endl;
		}

		//Ready to create the vulkan instance
		if (vkCreateInstance(&CreateInfo, nullptr, &mVkInstance) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Vulkan Instance!");
		}

	}

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice Device)
	{
		SwapChainSupportDetails Details;

		//Surface details capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, mSurface, &Details.mCapabilities);

		//Formats
		uint32_t FormatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(Device, mSurface, &FormatCount, nullptr);
		if (FormatCount != 0)
		{
			Details.mFormats.resize(FormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(Device, mSurface, &FormatCount, Details.mFormats.data());
		}

		//Presentation modes
		uint32_t PresentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(Device, mSurface, &PresentModeCount, nullptr);
		if (PresentModeCount != 0)
		{
			Details.mPresentModes.resize(PresentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(Device, mSurface, &PresentModeCount, Details.mPresentModes.data());
		}

		//Returning a std::vector is safe since the compiler will move-construct the new object (by actually moving the memory)
		return Details;
	}


	//Helper to choose the right swap chain surface format from the available ones
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& AvailableFormats)
	{
		if (AvailableFormats.size() == 1 && AvailableFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			//Alternate format (and also more accurate would be  VK_FORMAT_R8G8B8A8_SRGB)
			return { VK_FORMAT_B8G8R8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		for (const auto& AvailableFormat : AvailableFormats)
		{
			//Alternate format (and also more accurate would be  VK_FORMAT_R8G8B8A8_SRGB)
			if (AvailableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && AvailableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return AvailableFormat;
			}
		}

		return AvailableFormats[0];

	}

	//Present mode selection (trible/double or immediate presentation mode)
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& AvailablePresentModes)
	{
		//No all drivers will support this mode well, but we can leave it as last resort if no mailbox and immediate are available
		VkPresentModeKHR BestMode = VK_PRESENT_MODE_FIFO_KHR;

		for (const auto& AvailablePresentMode : AvailablePresentModes)
		{
			//Let's check if triple-buffering is available first
			if (AvailablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return AvailablePresentMode;
			}
			else if (AvailablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				BestMode = AvailablePresentMode;
			}
		}

		return BestMode;
	}

	//Let's chose the swap chain extent
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& Capabilities)
	{
		if (Capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return Capabilities.currentExtent;
		}
		else
		{
			VkExtent2D ActualExtent = { kScreenWidth, kScreenHeight };

			ActualExtent.width = std::max(Capabilities.minImageExtent.width, std::min(Capabilities.maxImageExtent.width, ActualExtent.width));

			ActualExtent.height = std::max(Capabilities.minImageExtent.height, std::min(Capabilities.maxImageExtent.height, ActualExtent.height));

			return ActualExtent;
		}
	}

	//Actual swap chain creation 
	void CreateSwapChain()
	{
		//Query for the swap chain support
		SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(mPhysicalDevice);

		//Choose the swap chain format
		VkSurfaceFormatKHR SurfaceFormat = ChooseSwapSurfaceFormat(SwapChainSupport.mFormats);

		//Select the present mode (ideally triple buffering. Which is MAILBOX present mode in Vulkan ) 
		VkPresentModeKHR PresentMode = ChooseSwapPresentMode(SwapChainSupport.mPresentModes);

		//Chose swap chain extent (in terms of resolution and so on)
		VkExtent2D Extent = ChooseSwapExtent(SwapChainSupport.mCapabilities);

		//Select the number of image the swap chain will be made of
		uint32_t ImageCount = SwapChainSupport.mCapabilities.minImageCount + 1;
		if (SwapChainSupport.mCapabilities.maxImageCount > 0 && ImageCount > SwapChainSupport.mCapabilities.maxImageCount)
		{
			ImageCount = SwapChainSupport.mCapabilities.maxImageCount;
		}

		//Ready to create the swap chain
		//Let's fill the usual struct
		VkSwapchainCreateInfoKHR CreateInfo = {};
		CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		CreateInfo.surface = mSurface;
		CreateInfo.minImageCount = ImageCount;
		CreateInfo.imageFormat = SurfaceFormat.format;
		CreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
		CreateInfo.imageExtent = Extent;
		CreateInfo.imageArrayLayers = 1;
		CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices Indices = FindQueueFamilies(mPhysicalDevice);
		uint32_t queueFamilyIndices[] = { (uint32_t)Indices.mGraphicsFamily, (uint32_t)Indices.mPresentFamily };

		if (Indices.mGraphicsFamily != Indices.mPresentFamily)
		{
			CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			CreateInfo.queueFamilyIndexCount = 2;
			CreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		}
		else
		{
			CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			CreateInfo.queueFamilyIndexCount = 0; // Optional
			CreateInfo.pQueueFamilyIndices = nullptr; // Optional			
		}

		CreateInfo.preTransform = SwapChainSupport.mCapabilities.currentTransform;
		CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		CreateInfo.presentMode = PresentMode;
		CreateInfo.clipped = VK_TRUE;

		//this filed will get important when it'll come the time to manage window resizing, infact we must assign the old swap chain on resizing when recreating the new one
		CreateInfo.oldSwapchain = VK_NULL_HANDLE;

		//Finally we can actually create the swap chain
		if (vkCreateSwapchainKHR(mDevice, &CreateInfo, nullptr, &mSwapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain!");
		}

		//We the retrieve the handles to the swap chain images. We will use these for rendering stuff into.
		uint32_t SwapChainImageCount = 0;
		vkGetSwapchainImagesKHR(mDevice, mSwapChain, &SwapChainImageCount, nullptr);
		mSwapChainImages.resize(SwapChainImageCount);
		vkGetSwapchainImagesKHR(mDevice, mSwapChain, &SwapChainImageCount, mSwapChainImages.data());

		//Store swap chain format and extent
		mSwapChainImageFormat = SurfaceFormat.format;
		mSwapChainExtent = Extent;

	}

	void CreateImageViews()
	{
		mSwapChainImageViews.resize(mSwapChainImages.size());
		auto ImagesCount = mSwapChainImages.size();
		for (size_t i = 0; i < ImagesCount; i++)
		{
			VkImageViewCreateInfo CreateInfo = {};
			CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			CreateInfo.image = mSwapChainImages[i];

			CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; //(1D, 2D, 3D Texture)
			CreateInfo.format = mSwapChainImageFormat;

			CreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			CreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			CreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			CreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			CreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			CreateInfo.subresourceRange.baseMipLevel = 0;
			CreateInfo.subresourceRange.levelCount = 1;
			CreateInfo.subresourceRange.baseArrayLayer = 0;
			CreateInfo.subresourceRange.layerCount = 1;

			//Let's create the image view for the i-th VkImage
			if (vkCreateImageView(mDevice, &CreateInfo, nullptr, &mSwapChainImageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create image views!");
			}

		}
	}

	//Helper to create a shader module on the fly
	VkShaderModule CreateShaderModule(const std::vector<char>& Code)
	{
		VkShaderModuleCreateInfo CreateInfo = {};

		CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		CreateInfo.codeSize = Code.size();
		CreateInfo.pCode = reinterpret_cast<const uint32_t*>(Code.data());

		VkShaderModule ShaderModule;
		if (vkCreateShaderModule(mDevice, &CreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shader module!");
		}
		return ShaderModule;
	}

	void CreateGraphicsPipeline()
	{
		//We load the shader bytecode
		auto VertexShaderCode = ReadFile("Shaders/vert.spv");
		auto FragmentShaderCode = ReadFile("Shaders/frag.spv");

		VkShaderModule VertexShaderModule;
		VkShaderModule FragmentShaderModule;

		//Shader modules creation
		VertexShaderModule = CreateShaderModule(VertexShaderCode);
		FragmentShaderModule = CreateShaderModule(FragmentShaderCode);

		//From now on all the necessary graphics pipeline stages will be created

		//Shader stages creation

		//VERTEX SHADER STAGE
		VkPipelineShaderStageCreateInfo VertShaderStageInfo = {};
		VertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		VertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		VertShaderStageInfo.module = VertexShaderModule;
		VertShaderStageInfo.pName = "main";

		//FRAGMENT SHADER STAGE
		VkPipelineShaderStageCreateInfo FragShaderStageInfo = {};
		FragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		FragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		FragShaderStageInfo.module = FragmentShaderModule;
		FragShaderStageInfo.pName = "main";

		//Fill shader stage array (used later in the during the actual graphics pipeline creation 
		VkPipelineShaderStageCreateInfo ShaderStages[] = { VertShaderStageInfo, FragShaderStageInfo };

		//Graphics pipeline stuff will be created here (i.e. vertex input layout structs, rasterizer structs and so on ... )

		//VERTEX INPUT LAYOUT (i.e. vertex element descriptor or similar in DX12)
		VkPipelineVertexInputStateCreateInfo VertexInputInfo = {};
		VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		VertexInputInfo.vertexBindingDescriptionCount = 0;
		VertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
		VertexInputInfo.vertexAttributeDescriptionCount = 0;
		VertexInputInfo.pVertexAttributeDescriptions = nullptr; //Optional

		//INPUT ASSEMBLY (Whether we want to draw triangle list, triangle strip, lines primitives etc.)
		VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo = {};
		InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		InputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		//Viewport
		VkViewport Viewport = {};
		Viewport.x = 0.0f;
		Viewport.y = 0.0f;
		Viewport.width = (float)mSwapChainExtent.width;
		Viewport.height = (float)mSwapChainExtent.height;
		Viewport.minDepth = 0.0f;
		Viewport.maxDepth = 1.0f;

		//Scissor rect
		VkRect2D Scissor = {};
		Scissor.offset = { 0, 0 };
		Scissor.extent = mSwapChainExtent;

		//VIEWPORT STATE 
		VkPipelineViewportStateCreateInfo ViewportState = {};
		ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		ViewportState.viewportCount = 1;
		ViewportState.pViewports = &Viewport;
		ViewportState.scissorCount = 1;
		ViewportState.pScissors = &Scissor;

		//RASTERIZER STATE
		VkPipelineRasterizationStateCreateInfo Rasterizer = {};
		Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		Rasterizer.depthClampEnable = VK_FALSE;
		Rasterizer.rasterizerDiscardEnable = VK_FALSE;
		Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		Rasterizer.lineWidth = 1.0f;
		Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		Rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		Rasterizer.depthBiasEnable = VK_FALSE;
		Rasterizer.depthBiasConstantFactor = 0.0f; // Optional
		Rasterizer.depthBiasClamp = 0.0f; // Optional
		Rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		//MULTISAMPLING
		VkPipelineMultisampleStateCreateInfo Multisampling = {};
		Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		Multisampling.sampleShadingEnable = VK_FALSE;
		Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		Multisampling.minSampleShading = 1.0f; // Optional
		Multisampling.pSampleMask = nullptr; // Optional
		Multisampling.alphaToCoverageEnable = VK_FALSE; // Optional		
		Multisampling.alphaToOneEnable = VK_FALSE; // Optional

		//COLOR BLEND ATTACHMENT STATE
		VkPipelineColorBlendAttachmentState ColorBlendAttachment = {};
		ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		ColorBlendAttachment.blendEnable = VK_FALSE;
		ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
		ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

		VkPipelineColorBlendStateCreateInfo ColorBlending = {};
		ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		ColorBlending.logicOpEnable = VK_FALSE;
		ColorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		ColorBlending.attachmentCount = 1;
		ColorBlending.pAttachments = &ColorBlendAttachment;
		ColorBlending.blendConstants[0] = 0.0f; // Optional
		ColorBlending.blendConstants[1] = 0.0f; // Optional
		ColorBlending.blendConstants[2] = 0.0f; // Optional
		ColorBlending.blendConstants[3] = 0.0f; // Optional


		//PIPELINE LAYOUT
		VkPipelineLayoutCreateInfo PipelineLayoutInfo = {};
		PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutInfo.setLayoutCount = 0; // Optional
		PipelineLayoutInfo.pSetLayouts = nullptr; // Optional
		PipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		PipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
		if (vkCreatePipelineLayout(mDevice, &PipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create pipeline layout!");
		}


		//FINALLY READY TO CREATE THE GRAPHICS PIPELINE !
		VkGraphicsPipelineCreateInfo PipelineInfo = {};
		PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		PipelineInfo.stageCount = 2;
		PipelineInfo.pStages = ShaderStages;

		PipelineInfo.pVertexInputState = &VertexInputInfo;
		PipelineInfo.pInputAssemblyState = &InputAssemblyInfo;
		PipelineInfo.pViewportState = &ViewportState;
		PipelineInfo.pRasterizationState = &Rasterizer;

		PipelineInfo.pMultisampleState = &Multisampling;
		PipelineInfo.pDepthStencilState = nullptr; // Optional
		PipelineInfo.pColorBlendState = &ColorBlending;
		PipelineInfo.pDynamicState = nullptr; // Optional

		//fixed function struct refs
		PipelineInfo.layout = mPipelineLayout;

		PipelineInfo.renderPass = mRenderPass;
		PipelineInfo.subpass = 0;

		//Used for graphics pipeline derivation
		PipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		PipelineInfo.basePipelineIndex = -1;              // Optional

		//Graphics Pipeline creation
		if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create graphics pipeline!");
		}

		//We destroy shader modules at the end of the pipeline creation
		vkDestroyShaderModule(mDevice, FragmentShaderModule, nullptr);
		vkDestroyShaderModule(mDevice, VertexShaderModule, nullptr);
	}

	void CreateRenderPass()
	{
		//Let's create the color attachment
		VkAttachmentDescription ColorAttachment = {};
		ColorAttachment.format = mSwapChainImageFormat;
		ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		//Color attachment
		VkAttachmentReference ColorAttachmentRef = {};
		ColorAttachmentRef.attachment = 0;
		ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		//Subpass
		VkSubpassDescription Subpass = {};
		Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		//The index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0) out vec4 outColor directive!
		Subpass.colorAttachmentCount = 1;
		Subpass.pColorAttachments = &ColorAttachmentRef;

		//Render Pass creation
		VkRenderPassCreateInfo RenderPassInfo = {};
		RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

		RenderPassInfo.attachmentCount = 1;
		RenderPassInfo.pAttachments = &ColorAttachment;
		RenderPassInfo.subpassCount = 1;
		RenderPassInfo.pSubpasses = &Subpass;

		if (vkCreateRenderPass(mDevice, &RenderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}

	}

	void CreateFramebuffers()
	{
		const auto ImageViewCount = mSwapChainImageViews.size();
		mSwapChainFramebuffers.resize(ImageViewCount);

		for (size_t i = 0; i < ImageViewCount; ++i)
		{
			VkImageView Attachments[] =
			{
				mSwapChainImageViews[i]
			};
			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = mRenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = Attachments;
			framebufferInfo.width = mSwapChainExtent.width;
			framebufferInfo.height = mSwapChainExtent.height;
			framebufferInfo.layers = 1;
			if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapChainFramebuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create framebuffer!");
			}
		}
	}

	void CreateCommandPool()
	{
		QueueFamilyIndices QFIndices = FindQueueFamilies(mPhysicalDevice);

		//Commnad pool info structs
		VkCommandPoolCreateInfo PoolInfo = {};
		PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		PoolInfo.queueFamilyIndex = QFIndices.mGraphicsFamily;
		PoolInfo.flags = 0; // Optional

		//Create the actual command pool
		if (vkCreateCommandPool(mDevice, &PoolInfo, nullptr, &mCommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create command pool!");
		}
	}

	void CreateCommandBuffers()
	{
		//Allocate as many command buffers as the number of swap chain images are
		mCommandBuffers.resize(mSwapChainFramebuffers.size());

		VkCommandBufferAllocateInfo AllocInfo = {};
		AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		AllocInfo.commandPool = mCommandPool;
		AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		AllocInfo.commandBufferCount = (uint32_t)mCommandBuffers.size();
		
		if (vkAllocateCommandBuffers(mDevice, &AllocInfo,mCommandBuffers.data()) != VK_SUCCESS)
		{
			 throw std::runtime_error("Failed to allocate command buffers!");				
		}

		//Command buffers recording 
		for (size_t i = 0; i < mCommandBuffers.size(); ++i) 
		{
			//Registering i-th command buffer
			VkCommandBufferBeginInfo BeginInfo = {};
			BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; //The command buffer can be resubmitted while it is also already pending execution.
			BeginInfo.pInheritanceInfo = nullptr; // Optional
			
			if(vkBeginCommandBuffer(mCommandBuffers[i], &BeginInfo) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to begin recording command buffer!");
			}	


			//Begin rendering starts with a begin render pass

			//But first we fill a render pass info struct
			VkRenderPassBeginInfo RenderPassInfo = {};
			RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			RenderPassInfo.renderPass = mRenderPass;
			RenderPassInfo.framebuffer = mSwapChainFramebuffers[i];

			//Render area must have the same extent of the swap chain images
			RenderPassInfo.renderArea.offset = { 0, 0 };
			RenderPassInfo.renderArea.extent = mSwapChainExtent;

			//Set the clear color
			VkClearValue ClearColor = { 1.0f, 0.0f, 0.0f, 1.0f };
			RenderPassInfo.clearValueCount = 1;
			RenderPassInfo.pClearValues = &ClearColor;

			//BEGIN RENDER PASS
			vkCmdBeginRenderPass(mCommandBuffers[i], &RenderPassInfo,VK_SUBPASS_CONTENTS_INLINE);

			//BIND THE GRAPHICS PIPELINE
			vkCmdBindPipeline(mCommandBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

			//Draw a triangle
			vkCmdDraw(mCommandBuffers[i], 3, 1, 0, 0);

			//END RENDER PASS
			vkCmdEndRenderPass(mCommandBuffers[i]);

			//We've finished recording this command buffer
			if (vkEndCommandBuffer(mCommandBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to record command buffer!");				
			}

		}
	}

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice Device)
	{
		QueueFamilyIndices Indices;

		uint32_t QueueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, QueueFamilies.data());

		int32_t i = 0;
		for (const auto& QueueFamily : QueueFamilies)
		{

			VkBool32 PresentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, mSurface,&PresentSupport);

			//Presentation Family Queue
			if (QueueFamily.queueCount > 0 && PresentSupport)
			{
				Indices.mPresentFamily = i;
			}

			//Graphics Family Queue
			if (QueueFamily.queueCount > 0 && QueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				Indices.mGraphicsFamily = i;				
			}

			if (Indices.IsComplete())
			{
				break;
			}

			++i;
		}

		return Indices;
	}


	bool CheckDeviceExtensionSupport(VkPhysicalDevice Device)
	{
		//Get the device extensions count
		uint32_t ExtensionCount = 0;
		vkEnumerateDeviceExtensionProperties(Device, nullptr,&ExtensionCount, nullptr);

		//Allocate space to store them
		std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);

		//Query for them again this time storing them in the previously allocated array
		vkEnumerateDeviceExtensionProperties(Device, nullptr,&ExtensionCount, AvailableExtensions.data());

		//Let's remove the available extensions from the list of the required ones, if the list will get empty then we'll know
		std::set<std::string> RequiredExtensions(DeviceExtensions.begin(),DeviceExtensions.end());
		for (const auto& extension : AvailableExtensions) 
		{
			RequiredExtensions.erase(extension.extensionName);			
		}		
		return RequiredExtensions.empty();
	}


	//Add meaningful checks here
	bool IsDeviceSuitable(VkPhysicalDevice Device) 
	{
        //Let's check if we support the queue family that we need
		auto Indices = FindQueueFamilies(Device);

		//Check the support for extensions supported by the physical device 
		bool ExtensionsSupported = CheckDeviceExtensionSupport(Device);

		//Check if the swap chain responds to the minimum requisites but only after we verified that the extensions are supported
		bool SwapChainAdequate = false;
		if (ExtensionsSupported) 
		{			
			SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(Device);			
			SwapChainAdequate = !SwapChainSupport.mFormats.empty() && !SwapChainSupport.mPresentModes.empty();			
		}

		//Add more features to test for in this function etc.
		return Indices.IsComplete() && ExtensionsSupported && SwapChainAdequate;
	}

	void PickPhysicalDevice() 
	{
		//We start by enumerating the graphics cards
		uint32_t DeviceCount = 0;
		vkEnumeratePhysicalDevices(mVkInstance, &DeviceCount, nullptr);
		if (DeviceCount == 0) 
		{
			throw std::runtime_error("Failed to find GPUs with Vulkan support!");
		}
		//We re-enumerate the physical device but this time we pass an array to fill
		std::vector<VkPhysicalDevice> Devices(DeviceCount);
		vkEnumeratePhysicalDevices(mVkInstance, &DeviceCount, Devices.data());
		for (const auto& Device : Devices) 
		{
			if (IsDeviceSuitable(Device)) 
			{
				mPhysicalDevice = Device;
				break;				
			}			
		}
	    if (mPhysicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("Failed to find a suitable GPU!");
		}
	}

	void CreateLogicalDevice()
	{
		/*
			The currently available drivers will only allow you to create a small number of
			queues for each queue family and you don’t really need more than one. That’s
			because you can create all of the command buffers on multiple threads and then
			submit them all at once on the main thread with a single low-overhead call.
		*/
		QueueFamilyIndices Indices = FindQueueFamilies(mPhysicalDevice);

		//Let's create a vector of queue for graphics and present queues respectively
		std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
        std::set<int32_t> UniqueQueueFamilies = { Indices.mGraphicsFamily, Indices.mPresentFamily };

		float QueuePriority = 1.0f;
		for (int queueFamily : UniqueQueueFamilies) 
		{
			 VkDeviceQueueCreateInfo QueueCreateInfo = {};
			 QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			 QueueCreateInfo.queueFamilyIndex = queueFamily;
			 QueueCreateInfo.queueCount = 1;
			 QueueCreateInfo.pQueuePriorities = &QueuePriority;
			 QueueCreateInfos.push_back(QueueCreateInfo);			
		}

		/*
			The next information to specify is the set of device features that we’ll
			be using. These are the features that we queried support for with
			vkGetPhysicalDeviceFeatures in the previous chapter, like geometry
			shaders. Right now we don’t need anything special, so we can simply define it
			and leave everything to VK_FALSE.
		*/
		VkPhysicalDeviceFeatures DeviceFeatures = {};

		VkDeviceCreateInfo CreateInfo = {};
		CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		CreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
		CreateInfo.queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfos.size());
		CreateInfo.pEnabledFeatures = &DeviceFeatures;

		//Enable extensions for this logical device 
		CreateInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
		CreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();

        //Check to see whether we should enable validation layers or not	
		if (kEnableValidationLayers) 
		{
			 CreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
			 CreateInfo.ppEnabledLayerNames = ValidationLayers.data();			
		}
		else
		{
			 CreateInfo.enabledLayerCount = 0;			
		}

		//We're ready to create the logical device out of the physical one
		if (vkCreateDevice(mPhysicalDevice, &CreateInfo, nullptr,&mDevice) != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to create logical device!");			
		}

		//Now we can retrieve a queue handle to the graphics family 
		vkGetDeviceQueue(mDevice, Indices.mGraphicsFamily, 0, &mGraphicsQueue);

		//Now we can get a handle to the present queue
		vkGetDeviceQueue(mDevice, Indices.mPresentFamily, 0, &mPresentQueue);

		/*
			With the logical device and queue handles we can now actually start using the
			graphics card to do things ! 
		*/

	}

	//Create a window surface
	void CreateSurface()
	{
		//We use GLFW multiplatform api to create a window in a platform agnostic way
		//We might have been using platform specific window creation function if we wanted (e.g. vkCreateWin32SurfaceKHR).
		if (glfwCreateWindowSurface(mVkInstance, mWindow, nullptr,&mSurface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface!");			
		}
	}

	void RecreateSwapChain() 
	{
		vkDeviceWaitIdle(mDevice);
		
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFramebuffers();
		CreateCommandBuffers();		
	}

	void InitVulkan()
	{
		CreateVulkanInstance();
		SetupDebugCallback();
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFramebuffers();
		CreateCommandPool();
		CreateCommandBuffers();
		CreateSynchObjects();
	}

	void CreateSynchObjects()
	{
		mImageAvailableSemaphores.resize(kMAX_FRAMES_IN_FLIGHT);
		mRenderFinishedSemaphores.resize(kMAX_FRAMES_IN_FLIGHT);
		mInFlightFences.resize(kMAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo SemaphoreInfo = {};
		SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo FenceInfo = {};
		FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		//Create two semaphores
		for (size_t i = 0; i < kMAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(mDevice, &SemaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS || 
				vkCreateSemaphore(mDevice, &SemaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(mDevice, &FenceInfo, nullptr,&mInFlightFences[i]))
			{
				throw std::runtime_error("Failed to create synchronization objects for a frame!");
			}					
		}

	}

	void DrawFrame()
	{
		//Wait for the GPU to finish the rendering of the current frame
		vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame],VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

		//Acquire an image from the swap chain
		uint32_t ImageIndex;
	    vkAcquireNextImageKHR(mDevice,mSwapChain,std::numeric_limits<uint64_t>::max(),mImageAvailableSemaphores[mCurrentFrame], VK_NULL_HANDLE, &ImageIndex);

		VkSubmitInfo SubmitInfo = {};
		SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		
		VkSemaphore WaitSemaphores[] = { mImageAvailableSemaphores[mCurrentFrame] };
		VkPipelineStageFlags WaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		SubmitInfo.waitSemaphoreCount = 1;
		SubmitInfo.pWaitSemaphores = WaitSemaphores;
		SubmitInfo.pWaitDstStageMask = WaitStages;

		//Execute the command buffer with that image as attachment in the framebuffer
		SubmitInfo.commandBufferCount = 1;
		SubmitInfo.pCommandBuffers = &mCommandBuffers[ImageIndex];

		VkSemaphore SignalSemaphores[] = { mRenderFinishedSemaphores[mCurrentFrame] };
		SubmitInfo.signalSemaphoreCount = 1;
		SubmitInfo.pSignalSemaphores = SignalSemaphores;

		//Submit the the command buffer to the graphics queue
		if ( vkQueueSubmit(mGraphicsQueue, 1, &SubmitInfo, mInFlightFences[mCurrentFrame] ) != VK_SUCCESS )
		{
			 throw std::runtime_error("Failed to submit draw command buffer!");				
		}

		//Create subpass dependency and get ready to pass it to the render pass
		VkSubpassDependency Dependency = {};
		Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		Dependency.dstSubpass = 0;
		Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		Dependency.srcAccessMask = 0;
		Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		//Render Pass info
		VkRenderPassCreateInfo RenderPassInfo = {};
		RenderPassInfo.dependencyCount = 1;
		RenderPassInfo.pDependencies = &Dependency;

		//Return the image to the swap chain for presentation
		VkPresentInfoKHR PresentInfo = {};
		PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		PresentInfo.waitSemaphoreCount = 1;
		PresentInfo.pWaitSemaphores = SignalSemaphores;

		VkSwapchainKHR SwapChains[] = { mSwapChain };
		PresentInfo.swapchainCount = 1;
		PresentInfo.pSwapchains = SwapChains;
		PresentInfo.pImageIndices = &ImageIndex;
		PresentInfo.pResults = nullptr; // Optional

		//Ready To Present a frame ! FINALLY !!!!!
		vkQueuePresentKHR(mPresentQueue, &PresentInfo);

		mCurrentFrame = (mCurrentFrame + 1) % kMAX_FRAMES_IN_FLIGHT;
	}

	void MainLoop()
	{
		while (!glfwWindowShouldClose(mWindow))
		{
			glfwPollEvents();
			DrawFrame();
		}

		//vkDeviceWaitIdle(mDevice); //<- not the optimal way of using the pipeline
	}

	void CleanUp()
	{	
		//Wait for the device to finish any pending rendering action before to destroy any potentially in use vulkan object/resource !
		vkDeviceWaitIdle(mDevice);

		//Destroy the two semaphores
		for (size_t i = 0; i < kMAX_FRAMES_IN_FLIGHT; i++) 
		{
			vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i],nullptr);
			vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i],nullptr);
			vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
		}
		
		//Destroy command pool
		vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

		//Destroy frame buffers
		for (auto Framebuffer : mSwapChainFramebuffers) 
		{
			vkDestroyFramebuffer(mDevice, Framebuffer, nullptr);
		}

		//Destroy the graphics pipeline
		vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);

		//Destroy pipeling layout
		vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);

		//Destroy Render pass
		vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

		//Destroy the image views we created out of the VkImage handles
		for (auto ImageView : mSwapChainImageViews) 
		{
			vkDestroyImageView(mDevice,ImageView, nullptr);
		}

		//Destroy the swap chain 
		vkDestroySwapchainKHR(mDevice,mSwapChain,nullptr);

		//Destroy the Vulkan logical device
		vkDestroyDevice(mDevice, nullptr);

		//Remove the debug messenger but only if the validation layers have been enabled
		if (kEnableValidationLayers)
		{
			 DestroyDebugUtilsMessengerEXT(mVkInstance, mCallback,nullptr);			
		}

		//Destroy the window surface 
		vkDestroySurfaceKHR(mVkInstance, mSurface, nullptr);

		//Destroy the Vulkan instance 
		vkDestroyInstance(mVkInstance, nullptr);

		//Destroy the already created window 
		glfwDestroyWindow(mWindow);

		//Terminate GLFW
		glfwTerminate();
	}

	//The GLFWindow to which we render into
	GLFWwindow* mWindow = nullptr;

	//Handle to a queue belonging to the graphics family (meaning that can hold and execute graphics commands only)
	VkQueue mGraphicsQueue = VK_NULL_HANDLE;

	//Handle to a queue belonging to the presentation family of queues
	VkQueue mPresentQueue = VK_NULL_HANDLE;

	//Our Vulkan instance
	VkInstance mVkInstance = nullptr;

	//Vulkan physical device (basically our graphics card)
	VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;

	//Vulkan logical device
	//You can even create multiple logical devices from the same physical device if you have varying requirements.
	VkDevice mDevice = VK_NULL_HANDLE;

	//Necessary to manage any debug callback in vulkan
	VkDebugUtilsMessengerEXT mCallback = nullptr;

	//Window Surface used to interface Vulkan with the window to whichi we want to render to
	VkSurfaceKHR mSurface = VK_NULL_HANDLE;

	//Swap chain
	VkSwapchainKHR mSwapChain = VK_NULL_HANDLE;

	//These will be the actual images contained in the swap chain that we'll reference for any rendering operation
	std::vector<VkImage> mSwapChainImages;

	//Swap chain image format
	VkFormat mSwapChainImageFormat;

	//Swap chain extent in terms of width and height
	VkExtent2D mSwapChainExtent;

	//Similar to DirectX12 we need to create a view for a given render target (in Vulkan VkImage), to know how to access that image (is it a 2D texture or a depth buffer ? and so on ...)
	std::vector<VkImageView> mSwapChainImageViews;

	//Swap chain framebuffers
	std::vector<VkFramebuffer> mSwapChainFramebuffers;

	//Render Pass 
	VkRenderPass mRenderPass;

	//Pipeline layout
	VkPipelineLayout mPipelineLayout;

	//Graphics pipeline
	VkPipeline mGraphicsPipeline;

	//COMMAND BUFFERS
	VkCommandPool mCommandPool;

	//We create a list of command buffers (one for each image of the swap chain)
	std::vector<VkCommandBuffer> mCommandBuffers;

	//Queue operations of draw command synchronization. We synchronize them using two semaphore

	//Semaphores are used for GPU-GPU synchronization
	//An image has been acquired and is ready for rendering (signal it!)	
	std::vector<VkSemaphore> mImageAvailableSemaphores;
		
	//Rendering has finished on the acquired image and is ready to be presented on screen (signal it!)	
	std::vector<VkSemaphore> mRenderFinishedSemaphores;

	//Fences are used for CPU-GPU synchronization
	std::vector<VkFence> mInFlightFences;

	//Current frame to be processed
	size_t mCurrentFrame = 0;

};



int main() 
{
	MyApplication App;

	App.Run();
	
	return 0;	
}


