#include <FileSystem/shader_files.h>

#include <string>
using std::string;



namespace RobotSimulator2020 {

const char* const shader_files::mlmm_global_vert = R"(

	#version 420 core
	layout (location = 0) in vec3 position;
	layout (location = 1) in vec3 normal;


	layout(std140, binding = 0) uniform MatrixBlock
	{
		mat4 projection;
		mat4 view;
	};
	uniform mat4 model;

	out vec3 Normal;
	out vec3 FragPos;


	void main()
	{
		gl_Position = projection * view *  model * vec4(position, 1.0f);
		FragPos = vec3(model * vec4(position, 1.0f));
		Normal = mat3(transpose(inverse(model))) * normal;  
	} 

)";

const char* const shader_files::mlmm_global_frag = R"(

	#version 420 core

	//	Use uniform buffer
	const int MaxLights = 10;
	layout (std140, binding = 2) uniform LightInfo{
		bool isEnabled;
		int lightType;

		float cutOff;
		float outerCutOff;

		vec3 position;
		vec3 direction;

		float constant;
		float linear;
		float quadratic;
  
		vec4 ambient;
		vec4 diffuse;
		vec4 specular;       
	}Lights[MaxLights];

	const int MaxMaterials = 3;
	layout (std140, binding = 12) uniform MaterialInfo{
		vec4 emission;
		vec4 ambient;
		vec4 diffuse;
		vec4 specular;
		float shininess;
	}Materials[MaxMaterials];
	uniform int current_material;

	//	Fragment position and normal from vertex shader
	in vec3 FragPos;
	in vec3 Normal;

	//	Fragment shader output
	out vec4 color;

	//	Position of the view is needed in this shader
	uniform vec3 viewPos;


	void main()
	{    
		// Properties
		vec3 norm = normalize(Normal);
		vec3 viewDir = normalize(viewPos - FragPos);
    
		// == ======================================
		// Our lighting is set up in 3 phases: directional, point lights and an optional flashlight
		// For each phase, a calculate function is defined that calculates the corresponding color
		// per lamp. In the main() function we take all the calculated colors and sum them up for
		// this fragment's final color.
		// == ======================================
	
		// The variable is defined in vec4 in order to use the transparent value.
		vec4 result = vec4(0.0f); 

		//	Go through all the lights in the environment
		for (int light = 0; light < MaxLights; ++light) {
			//	Check whether the lights are enabled
			if (!Lights[light].isEnabled)
				continue;

			//	1. Directional lighting
			if (Lights[light].lightType == 0){
				//	Light direction is the opposite of light direction
				vec3 lightDir = normalize(-Lights[light].direction);
				//	Diffuse shading
				float diff = max(dot(norm, lightDir), 0.0);
				//	Specular shading
				vec3 reflectDir = reflect(-lightDir, norm);
				float spec = pow(max(dot(viewDir, reflectDir), 0.0), Materials[current_material].shininess);

				// Combine results
				vec4 ambient = Lights[light].ambient * Materials[current_material].ambient;
				vec4 diffuse = Lights[light].diffuse * diff * Materials[current_material].diffuse;
				vec4 specular = Lights[light].specular * spec * Materials[current_material].specular;
				result += (ambient + diffuse + specular);
			}
			//	2. Point lights
			else if (Lights[light].lightType == 1){
				//	Light direction of point light should be calculated
				//	Direction is from the position of light to the position of the corresponding fragment
				vec3 lightDir = normalize(Lights[light].position - FragPos);
				//	Diffuse shading
				float diff = max(dot(norm, lightDir), 0.0);
				//	Specular shading
				vec3 reflectDir = reflect(-lightDir, norm);
				float spec = pow(max(dot(viewDir, reflectDir), 0.0), Materials[current_material].shininess);

				// Attenuation
				float distance = length(Lights[light].position - FragPos);
				float attenuation = 1.0f / (Lights[light].constant + Lights[light].linear * distance + Lights[light].quadratic * (distance * distance));    
			
				// Combine results
				vec4 ambient = Lights[light].ambient * Materials[current_material].ambient;
				vec4 diffuse = Lights[light].diffuse * diff * Materials[current_material].diffuse;
				vec4 specular = Lights[light].specular * spec * Materials[current_material].specular;
				ambient *= attenuation;
				diffuse *= attenuation;
				specular *= attenuation;
				result += (ambient + diffuse + specular);
			}
			//	3. Spot light
			else if (Lights[light].lightType == 2){
				//	Light direction of point light should be calculated
				//	Direction is from the position of light to the position of the corresponding fragment
				vec3 lightDir = normalize(Lights[light].position - FragPos);
				//	Diffuse shading
				float diff = max(dot(norm, lightDir), 0.0);
				//	Specular shading
				vec3 reflectDir = reflect(-lightDir, norm);
				float spec = pow(max(dot(viewDir, reflectDir), 0.0), Materials[current_material].shininess);

				//	Attenuation
				float distance = length(Lights[light].position - FragPos);
				float attenuation = 1.0f / (Lights[light].constant + Lights[light].linear * distance + Lights[light].quadratic * (distance * distance));    
				//	Spotlight intensity
				float theta = dot(lightDir, normalize(-Lights[light].direction)); 
				float epsilon = Lights[light].cutOff - Lights[light].outerCutOff;
				float intensity = clamp((theta - Lights[light].outerCutOff) / epsilon, 0.0, 1.0);

				// Combine results
				vec4 ambient = Lights[light].ambient * Materials[current_material].ambient;
				vec4 diffuse = Lights[light].diffuse * diff * Materials[current_material].diffuse;
				vec4 specular = Lights[light].specular * spec * Materials[current_material].specular;
				ambient *= attenuation * intensity;
				diffuse *= attenuation * intensity;
				specular *= attenuation * intensity;
				result += (ambient + diffuse + specular);
			}
		}

		color = result + Materials[current_material].emission;
	}

)";






}
