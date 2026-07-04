// New-API port of Examples_old/js/soft-body-creator.js. Builds SoftBodySharedSettings
// for cloths, spheres and cubes using the ergonomic embind API (AddVertex/AddFace/
// AddEdgeConstraint/AddVolumeConstraint + CreateConstraints), so the raw Vertex/Face/
// Edge/Volume structs and Array push_back of the original are hidden. Each function
// takes the initialized Jolt module as its first argument.

// A grid cloth. invMassFn(x,z) returns the inverse mass (0 pins a vertex); perturbFn(x,z)
// returns a small {x,y,z} offset. bendType is a Jolt.EBendType; lraType a Jolt.ELRAType.
export function createCloth(Jolt, gridX = 30, gridZ = 30, spacing = 0.75,
	invMassFn = () => 1, perturbFn = () => ({ x: 0, y: 0, z: 0 }),
	bendType = Jolt.EBendType.None, lraType = Jolt.ELRAType.None, lraMaxDistanceMultiplier = 1.0,
	compliance = 1.0e-5) {
	const offX = -0.5 * spacing * (gridX - 1), offZ = -0.5 * spacing * (gridZ - 1);
	const ss = new Jolt.SoftBodySharedSettings();
	for (let z = 0; z < gridZ; ++z)
		for (let x = 0; x < gridX; ++x) {
			const p = perturbFn(x, z);
			ss.AddVertex([spacing * x + offX + p.x, p.y, spacing * z + offZ + p.z], invMassFn(x, z));
		}
	const vidx = (x, y) => x + y * gridX;
	ss.CalculateEdgeLengths();
	for (let z = 0; z < gridZ - 1; ++z)
		for (let x = 0; x < gridX - 1; ++x) {
			ss.AddFace(vidx(x, z), vidx(x, z + 1), vidx(x + 1, z + 1));
			ss.AddFace(vidx(x, z), vidx(x + 1, z + 1), vidx(x + 1, z));
		}
	ss.CreateConstraints(compliance, compliance, compliance, bendType, lraType, lraMaxDistanceMultiplier);
	ss.Optimize();
	return ss;
}

export function createClothWithFixatedCorners(Jolt, gridX = 30, gridZ = 30, spacing = 0.75) {
	const inv_mass = (x, z) => (x == 0 && z == 0) || (x == gridX - 1 && z == 0)
		|| (x == 0 && z == gridZ - 1) || (x == gridX - 1 && z == gridZ - 1) ? 0.0 : 1.0;
	return createCloth(Jolt, gridX, gridZ, spacing, inv_mass);
}

// A solid cube built from edge + volume (tetrahedron) constraints, with surface faces.
export function createCube(Jolt, gridSize = 5, spacing = 0.5, edgeCompliance = 0, volumeCompliance = 0) {
	const off = -0.5 * spacing * (gridSize - 1);
	const ss = new Jolt.SoftBodySharedSettings();
	for (let z = 0; z < gridSize; ++z)
		for (let y = 0; y < gridSize; ++y)
			for (let x = 0; x < gridSize; ++x)
				ss.AddVertex([spacing * x + off, spacing * y + off, spacing * z + off], 1.0);

	const vidx = (x, y, z) => x + y * gridSize + z * gridSize * gridSize;
	for (let z = 0; z < gridSize; ++z)
		for (let y = 0; y < gridSize; ++y)
			for (let x = 0; x < gridSize; ++x) {
				const v0 = vidx(x, y, z);
				if (x < gridSize - 1) ss.AddEdgeConstraint(v0, vidx(x + 1, y, z), edgeCompliance);
				if (y < gridSize - 1) ss.AddEdgeConstraint(v0, vidx(x, y + 1, z), edgeCompliance);
				if (z < gridSize - 1) ss.AddEdgeConstraint(v0, vidx(x, y, z + 1), edgeCompliance);
			}
	ss.CalculateEdgeLengths();

	const tetra = [
		[[0, 0, 0], [0, 1, 1], [0, 0, 1], [1, 1, 1]],
		[[0, 0, 0], [0, 1, 0], [0, 1, 1], [1, 1, 1]],
		[[0, 0, 0], [0, 0, 1], [1, 0, 1], [1, 1, 1]],
		[[0, 0, 0], [1, 0, 1], [1, 0, 0], [1, 1, 1]],
		[[0, 0, 0], [1, 1, 0], [0, 1, 0], [1, 1, 1]],
		[[0, 0, 0], [1, 0, 0], [1, 1, 0], [1, 1, 1]],
	];
	for (let z = 0; z < gridSize - 1; ++z)
		for (let y = 0; y < gridSize - 1; ++y)
			for (let x = 0; x < gridSize - 1; ++x)
				for (let t = 0; t < 6; ++t) {
					const o = tetra[t];
					ss.AddVolumeConstraint(
						vidx(x + o[0][0], y + o[0][1], z + o[0][2]), vidx(x + o[1][0], y + o[1][1], z + o[1][2]),
						vidx(x + o[2][0], y + o[2][1], z + o[2][2]), vidx(x + o[3][0], y + o[3][1], z + o[3][2]), volumeCompliance);
				}
	ss.CalculateVolumeConstraintVolumes();

	// Surface faces (6 sides)
	const n = gridSize - 1;
	for (let y = 0; y < n; ++y)
		for (let x = 0; x < n; ++x) {
			ss.AddFace(vidx(x, y, 0), vidx(x, y + 1, 0), vidx(x + 1, y + 1, 0));
			ss.AddFace(vidx(x, y, 0), vidx(x + 1, y + 1, 0), vidx(x + 1, y, 0));
			ss.AddFace(vidx(x, y, n), vidx(x + 1, y + 1, n), vidx(x, y + 1, n));
			ss.AddFace(vidx(x, y, n), vidx(x + 1, y, n), vidx(x + 1, y + 1, n));
			ss.AddFace(vidx(x, 0, y), vidx(x + 1, 0, y + 1), vidx(x, 0, y + 1));
			ss.AddFace(vidx(x, 0, y), vidx(x + 1, 0, y), vidx(x + 1, 0, y + 1));
			ss.AddFace(vidx(x, n, y), vidx(x, n, y + 1), vidx(x + 1, n, y + 1));
			ss.AddFace(vidx(x, n, y), vidx(x + 1, n, y + 1), vidx(x + 1, n, y));
			ss.AddFace(vidx(0, x, y), vidx(0, x, y + 1), vidx(0, x + 1, y + 1));
			ss.AddFace(vidx(0, x, y), vidx(0, x + 1, y + 1), vidx(0, x + 1, y));
			ss.AddFace(vidx(n, x, y), vidx(n, x + 1, y + 1), vidx(n, x, y + 1));
			ss.AddFace(vidx(n, x, y), vidx(n, x + 1, y), vidx(n, x + 1, y + 1));
		}
	ss.Optimize();
	return ss;
}

// A pressurized sphere. Deliberately uses uneven polar vertices (like the original) to
// exercise the pressure solver with non-uniform triangles.
export function createSphere(Jolt, radius = 1, numTheta = 10, numPhi = 20,
	bendType = Jolt.EBendType.None, compliance = 1.0e-4, bendCompliance = 1.0e-3) {
	const ss = new Jolt.SoftBodySharedSettings();
	// THREE-style spherical coords: phi from +Y (polar), theta azimuthal
	const spherical = (phi, theta) => {
		const s = radius * Math.sin(phi);
		return [s * Math.sin(theta), radius * Math.cos(phi), s * Math.cos(theta)];
	};
	ss.AddVertex(spherical(0, 0), 1.0);
	ss.AddVertex(spherical(Math.PI, 0), 1.0);
	for (let theta = 1; theta < numTheta - 1; ++theta)
		for (let phi = 0; phi < numPhi; ++phi)
			ss.AddVertex(spherical(Math.PI * theta / (numTheta - 1), 2.0 * Math.PI * phi / numPhi), 1.0);

	const vidx = (theta, phi) => theta == 0 ? 0 : theta == numTheta - 1 ? 1 : 2 + (theta - 1) * numPhi + phi % numPhi;
	for (let phi = 0; phi < numPhi; ++phi) {
		for (let theta = 0; theta < numTheta - 2; ++theta) {
			ss.AddFace(vidx(theta, phi), vidx(theta + 1, phi), vidx(theta + 1, phi + 1));
			if (theta > 0) ss.AddFace(vidx(theta, phi), vidx(theta + 1, phi + 1), vidx(theta, phi + 1));
		}
		ss.AddFace(vidx(numTheta - 2, phi + 1), vidx(numTheta - 2, phi), vidx(numTheta - 1, 0));
	}
	ss.CreateConstraints(compliance, compliance, bendCompliance, bendType, Jolt.ELRAType.None, 1.0);
	ss.Optimize();
	return ss;
}
