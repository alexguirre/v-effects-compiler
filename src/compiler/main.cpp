#include <iostream>
#include <string>
#include <memory>
#include "Effect.h"
#include "EffectSaver.h"

int main()
{
	std::string src(R"(
	cbuffer rage_matrices : register(b0)
	{
	  row_major float4x4 gWorld;         // Offset:    0 Size:    64
	  row_major float4x4 gWorldView;     // Offset:   64 Size:    64
	  row_major float4x4 gWorldViewProj; // Offset:  128 Size:    64
	  row_major float4x4 gViewInverse;   // Offset:  192 Size:    64
	}

	cbuffer LaserParam : register(b1)
	{
		float gMaxDisplacement;            // Offset:    0 Size:     4
		float gCameraDistanceAtMaxDisplacement;// Offset:    4 Size:     4
		float4 LaserVisibilityMinMax;      // Offset:   16 Size:    16
	}

	struct VS_LaserBeam_Input
	{
		float3 position : POSITION;
		float3 normal : NORMAL;
		float4 texCoord0 : TEXCOORD0;
		float4 texCoord1 : TEXCOORD1;
		float4 texCoord2 : TEXCOORD2;
	};

	struct VS_LaserBeam_Output
	{
		float4 position : SV_Position;
		float3 texCoord0 : TEXCOORD0;
		float3 texCoord1 : TEXCOORD1;
		float2 texCoord2 : TEXCOORD2;
		float4 texCoord3 : TEXCOORD3;
		float4 texCoord4 : TEXCOORD4;
	};

	VS_LaserBeam_Output VS_LaserBeam(VS_LaserBeam_Input i)
	{
		VS_LaserBeam_Output o = (VS_LaserBeam_Output)0;
		float4 r0;
    
		r0 = i.position.yyyy * gWorldViewProj[1].xyzw;
		r0 = i.position.xxxx * gWorldViewProj[0].xyzw + r0.xyzw;
		r0 = i.position.zzzz * gWorldViewProj[2].xyzw + r0.xyzw;
		r0 = r0.xyzw + gWorldViewProj[3].xyzw;
		o.position = r0;
		o.texCoord3.w = r0.z;
		o.texCoord0.xyz = i.position.xyz + -gViewInverse[3].xyz;
		o.texCoord1.xyz = i.normal.xyz;
		o.texCoord2.xy = i.texCoord0.xy;
		r0.x = i.position.y * gWorldView[1].z;
		r0.x = i.position.x * gWorldView[0].z + r0.x;
		r0.x = i.position.z * gWorldView[2].z + r0.x;
		r0.x = r0.x + gWorldView[3].z;
		o.texCoord3.z = -r0.x;
		o.texCoord3.xy = i.texCoord2.xy;
		o.texCoord4.xyzw = i.texCoord1.xyzw;
		return o;
	}

	float4 PS_LaserBeam(VS_LaserBeam_Output input) : SV_Target
	{
		float4 r0, r1;
    
		r0.x = max(LaserVisibilityMinMax.x, 0.0f);
		r0.x = min(r0.x, LaserVisibilityMinMax.y);
		r0.y = -abs(input.texCoord3.y) * abs(input.texCoord3.y) + 1.0f; 
		r1 = r0.y * input.texCoord4;
		r0 = r0.x * r1;
		r1.x = dot(r0.xyz, r0.www);
		r0.xyz = r1.xxx * float3(0.5f,0.5f,0.5f) + r0.xyz;
		r1.x = saturate(input.texCoord3.w * 5.0f);
		return r0.xyzw * r1.xxxx;
	}

	technique LaserBeam
	{
		pass
		{
			VertexShader = VS_LaserBeam;
			PixelShader = PS_LaserBeam;

			// Rasterizer State
			FillMode = SOLID;
			CullMode = NONE;

			// Depth-Stencil State
			DepthEnable = TRUE;
			DepthWriteMask = ALL;
			DepthFunc = GREATER_EQUAL;
			StencilEnable = FALSE;
			StencilWriteMask = 255;
			StencilReadMask = 0xFF;
			FrontFaceStencilFail = KEEP;
			FrontFaceStencilDepthFail = KEEP;
			FrontFaceStencilPass = REPLACE;
			FrontFaceStencilFunc = ALWAYS;
			BackFaceStencilFail = KEEP;
			BackFaceStencilDepthFail = KEEP;
			BackFaceStencilPass = REPLACE;
			BackFaceStencilFunc = ALWAYS;

			// Blend State
			AlphaToCoverageEnable = FALSE;
			BlendEnable0 = TRUE;
			SrcBlend0 = SRC_ALPHA;
			DestBlend0 = ONE;
			BlendOp0 = ADD;
			SrcBlendAlpha0 = SRC_ALPHA;
			DestBlendAlpha0 = INV_SRC_ALPHA;
			BlendOpAlpha0 = ADD;
			RenderTargetWriteMask0 = 15; 
		}
	}


	technique TestTech // a comment
	{ // a comment
		pass
		{
			firstentry=NONE;second=TRUE;
			third	  = FILL;
		} // a comment
	}

	technique  // a comment
	Second
	{
		pass
		{
			SomeState = GREATER_EQUAL;
			OtherState=GREATER;
			SomeOtherState = FALSE;
		}
		pass
		{
			SecondSomeState     =   LESS_EQUAL;
			SecondOtherState	=	LESS;
			SecondSomeOtherState=TRUE;
		}
	}



		technique Third{pass{TheState=TRUE;AndAnotherState=GREATER;}pass{S=NONE;}}
	)");

	std::unique_ptr<CEffect> fx = std::make_unique<CEffect>(src);

	std::cout << "Preprocessed source:\n";
	std::cout << fx->PreprocessSource() << "\n";

	std::cout << "Techniques\n";
	for (auto& t : fx->Techniques())
	{
		std::cout << "	Name:" << t.Name << "\n";
		std::cout << "	Passes (" << t.Passes.size() << "):\n";
		for (auto& p : t.Passes)
		{
			std::cout << "		Pass\n";
			for (auto& a : p.Assigments)
			{
				std::cout << "			" << a.Type << " = " << a.Value << "\n";
			}
		}
	}

	try
	{
		std::cout << "Getting 'VS_LaserBeam'...\n";
		const CCodeBlob& vsCode = fx->GetProgramCode("VS_LaserBeam");
		std::cout << "	Size:" << vsCode.Size() << "\n";

		std::cout << "Getting 'PS_LaserBeam'...\n";
		const CCodeBlob& psCode = fx->GetProgramCode("PS_LaserBeam");
		std::cout << "	Size:" << psCode.Size() << "\n";
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
	
	CEffectSaver s(*fx);
	try
	{
		s.SaveTo("test.fxc");
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
