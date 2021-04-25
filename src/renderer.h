#pragma once
#include "prefab.h"

//forward declarations
class Camera;

namespace GTR {

	class Prefab;
	class Light;
	class Material;
	
	class RenderCall
	{
	public:
		float distanceCamera;
		bool isTransparent;

		Matrix44 model;
		GTR::Node* node;
		
		RenderCall(const Matrix44 model, GTR::Node* node, float distanceCamera);
	};
	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{

	public:
		std::vector<RenderCall*> calls;


		void renderCalls(Camera* camera);
		void sortCalls();
		struct {
			bool operator()(RenderCall* a, RenderCall* b) const {
				if (!b->isTransparent)
					return false;
				if (!a->isTransparent)
					return true;
				return a->distanceCamera > b->distanceCamera;
			}
		} comparator;

		//renders several elements of the scene
		void renderScene(GTR::Scene* scene, Camera* camera);
	
		//to render a whole prefab (with all its nodes)
		void renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera);

		//to render one node from the prefab and its children
		void renderNode(const Matrix44& model, GTR::Node* node, Camera* camera);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
	};


	Texture* CubemapFromHDRE(const char* filename);

};