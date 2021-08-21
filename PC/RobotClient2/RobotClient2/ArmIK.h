#pragma once

#include <vector>

using namespace std;

struct Joint
{
	float xpos, ypos;
	float len;
	float localAngle;
	float globalAngle;
	bool limited;
	float minAngle;
	float maxAngle;
};

class ArmIK
{
public:
	vector<Joint> joints;
	float xEnd, yEnd;

	void AddJoint(float len);
	void AddJoint(float len, float minAngle, float maxAngle);
	void ForwardKinematics(int startJoint);
	void InverseKinematics(float xTarget, float yTarget, int steps);
};
