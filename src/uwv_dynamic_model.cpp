/***************************************************************************/
/*  Dynamic model for an underwater vehicle	                           */
/*                                                                         */
/* FILE --- uwv_dynamic_model.cpp	                                   */
/*                                                                         */
/* PURPOSE --- Source file for a Dynamic model of an 	                   */
/*             underwater vehicle. Based on T.I.Fossen & Giovanni Indiveri */
/*                                                                         */
/*  Sankaranarayanan Natarajan                                             */
/*  sankar.natarajan@dfki.de                                               */
/*  DFKI - BREMEN 2011                                                     */
/***************************************************************************/

#include "uwv_dynamic_model.h"

using namespace std;

namespace underwaterVehicle
{
	//DynamicModel(double samp_time , int _simulation_per_cycle , double _initial_time , double *_initial_state );
	//RK4_SIM(int _plant_order, int _ctrl_order, double _integration_step , double _initial_time , double *_initial_state )				
	DynamicModel::DynamicModel(double _sampling_period, int _simulation_per_cycle, double _initial_time, double *_initial_state, int _plant_order, int _ctrl_order)		
		: RK4_SIM(_plant_order, _ctrl_order, (_sampling_period/(double)_simulation_per_cycle), _initial_time, _initial_state)		
	{ 	
	}

	DynamicModel::~DynamicModel()
	{ }

	void DynamicModel::init_param(underwaterVehicle::Parameters _param)
	{		
		// initialising the following Matrices and Vectors to Zero		
		mass_matrix_bff		= Eigen::Matrix<double, DOF, DOF>	::Zero();
		mass_matrix_eff		= Eigen::Matrix<double, DOF, DOF>	::Zero();
		Inv_massMatrix 		= Eigen::Matrix<double, DOF, DOF>	::Zero();
		gravitybuoyancy_bff	= Eigen::Matrix<double, DOF, 1>		::Zero();
		gravitybuoyancy_eff	= Eigen::Matrix<double, DOF, 1>		::Zero();
		damping_matrix_bff    	= Eigen::Matrix<double, DOF, DOF>	::Zero();
		damping_matrix_eff    	= Eigen::Matrix<double, DOF, DOF>	::Zero();
		velocity       		= Eigen::Matrix<double, DOF, 1>		::Zero();
		acceleration   		= Eigen::Matrix<double, DOF, 1>		::Zero();
		orientation_euler	= Eigen::Vector3d			::Zero();
		position		= Eigen::Vector3d			::Zero();
		linear_velocity		= Eigen::Vector3d			::Zero();
		angular_velocity	= Eigen::Vector3d			::Zero();
		linear_acceleration	= Eigen::Vector3d			::Zero();
		angular_acceleration	= Eigen::Vector3d			::Zero();
		rot_BI			= Eigen::Matrix3d			::Zero();
		rot_IB			= Eigen::Matrix3d			::Zero();
		jacob_kin_e		= Eigen::Matrix3d			::Zero();
		inv_jacob_kin_e 	= Eigen::Matrix3d			::Zero();
		inv_jacob_e_RIB		= Eigen::Matrix<double, 6, 6>		::Zero();
		invTrans_jacob_e_RIB	= Eigen::Matrix<double, 6, 6>		::Zero();	
		zero3x3			= Eigen::Matrix3d			::Zero();

		uwv_weight		= 0.0;
		uwv_buoyancy		= 0.0;
				
		orientation_quaternion.w() = 1.0;
		orientation_quaternion.x() = 0.0;
		orientation_quaternion.y() = 0.0;
		orientation_quaternion.z() = 0.0;
		
		// # assigning the damping parameter is done in a function
		// # since its value depends on the vehicle direction
		
		for( int i = 0; i < DOF; i++)
		{
			param.linDampCoeff[i].positive  = 0.0;
			param.linDampCoeff[i].negative  = 0.0;
			param.quadDampCoeff[i].positive = 0.0;
			param.quadDampCoeff[i].negative = 0.0;

		}

		// input vehicle parameter		
		param = _param;		

		number_of_thrusters = param.thrusters.thruster_value.size();	
	
		dc_volt.resize(number_of_thrusters, 0.0);
		
		input_thrust		= Eigen::MatrixXd::Zero(number_of_thrusters,1);
		thrust_bff		= Eigen::MatrixXd::Zero(DOF,1);
		thrust_eff		= Eigen::MatrixXd::Zero(DOF,1);		
		thruster_control_matrix = Eigen::MatrixXd::Zero(DOF, number_of_thrusters);

		temp_thrust		= Eigen::MatrixXd::Zero(DOF,0.0);

		// # assigning the mass matrix values from the input
		// # currently it is done in a function
		// # since its value depends on the vehicle direction
		/*int massmatrix_ct = 0;
		for(int i = 0; i < DOF; i++)
		{
			for(int j = 0; j < DOF; j++)
			{
				mass_matrix(j,i) = param.mass_matrix[massmatrix_ct];
				massmatrix_ct = massmatrix_ct +1;
			}			
		}*/

		
		// assigning the thruster_control_matrix values from the input
		int thrustercontrolmatrix_ct = 0;
		for(int i = 0; i < number_of_thrusters; i++)
		{
			for(int j = 0; j < DOF; j++)
			{
				thruster_control_matrix(j,i) = param.thruster_control_matrix[thrustercontrolmatrix_ct];				
				thrustercontrolmatrix_ct = thrustercontrolmatrix_ct +1;
			}			
		}				

		// uwv physical parameters 										
		uwv_weight = param.uwv_mass * param.gravity; 						//W=mass*gravity
		uwv_buoyancy = param.waterDensity * param.uwv_volume * param.gravity; 			//Buoyancy = density * volume * gravity - density of pure water at 20�C in kg/m^3 is 998.2 - volume of cylinder (lwh)	

		negative_constant = 1e-6;
	}
	

	Eigen::Vector3d DynamicModel::getAcceleration()
	{
		//Eigen ::Vector3d _acceleration(0.0, 0.0, 0.0);

		//_acceleration(0) = acceleration(0);
		//_acceleration(1) = acceleration(1);
		//_acceleration(2) = acceleration(2);
		
		//return _acceleration;	
		
		// can alos use acceleration.head(3) - efficiency ??
		return linear_acceleration;
	}

	Eigen::Vector3d DynamicModel::getPosition()
	{		
		return position;
	}

	Eigen::Quaterniond DynamicModel::getOrientation_in_Quat()
	{		
		
		orientation_quaternion = Euler_to_Quaternion(orientation_euler);

		return orientation_quaternion;
	}

	Eigen::Vector3d DynamicModel::getOrientation_in_Euler()
	{
		return orientation_euler;
	}

	Eigen::Vector3d DynamicModel::getAngularVelocity()
	{
		//Eigen ::Vector3d _angularvelocity(0.0, 0.0, 0.0);

		//_angularvelocity(0) = plant_state[3];
		//_angularvelocity(1) = plant_state[4];
		//_angularvelocity(2) = plant_state[5];
		
		//return _angularvelocity;
		
		return angular_velocity;
	}

	double DynamicModel::Simulationtime()
	{		
		return (double)current_time;
	}

	Eigen::Vector3d DynamicModel::getLinearVelocity()
	{
		//Eigen ::Vector3d _velocity(0.0, 0.0, 0.0);

		//_velocity(0) = velocity(0);
		//_velocity(1) = velocity(1);
		//_velocity(2) = velocity(2);
		
		//return _velocity;		
		
		return linear_velocity;
	}

	Eigen::Vector3d DynamicModel::Quaternion_to_Euler(Eigen::Quaternion<double> q)
	{
		Eigen::Vector3d res(0.0, 0.0, 0.0);
		
		res(0) = atan2( (2*((q.w()+q.x())+(q.y()+q.z()))), (1-(2*( sq(q.x())+sq(q.y()) ))) );
		res(1) = asin( 2*( (q.w() * q.y())-(q.z() * q.x()) ));
		res(2) = atan2( (2*((q.w()+q.z())+(q.x()+q.y()))), (1-(2*( sq(q.y())+sq(q.z()) ))) );	

		return res;
	}

	Eigen::Quaternion<double> DynamicModel::Euler_to_Quaternion(const Eigen::Vector3d &eulerang)
	{
		Eigen::Quaternion<double> res(1,0,0,0);

		res.w() = ( cos(eulerang(0)/2)*cos(eulerang(1)/2)*cos(eulerang(2)/2) ) + ( sin(eulerang(0)/2)*sin(eulerang(1)/2)*sin(eulerang(2)/2) );
		res.x() = ( sin(eulerang(0)/2)*cos(eulerang(1)/2)*cos(eulerang(2)/2) ) - ( cos(eulerang(0)/2)*sin(eulerang(1)/2)*sin(eulerang(2)/2) );
		res.y() = ( cos(eulerang(0)/2)*sin(eulerang(1)/2)*cos(eulerang(2)/2) ) + ( sin(eulerang(0)/2)*cos(eulerang(1)/2)*sin(eulerang(2)/2) );
		res.z() = ( cos(eulerang(0)/2)*cos(eulerang(1)/2)*sin(eulerang(2)/2) ) - ( sin(eulerang(0)/2)*sin(eulerang(1)/2)*cos(eulerang(2)/2) );		

		return res;
	}

	void DynamicModel::inertia_matrix(const Eigen::Matrix<double,6,1> &velocity,Eigen::Matrix<double,6,6>& mass_matrix)
	{	

		for (int i = 0; i < DOF; i++)
		{
			if (velocity(i) > negative_constant)
				mass_matrix(i,i) = param.massCoefficient[i].positive;
			else
				mass_matrix(i,i) = param.massCoefficient[i].negative;

		}			
		
	}
				

	void DynamicModel::hydrodynamic_damping(const Eigen::Matrix<double,6,1> &velocity,Eigen::Matrix<double,6,6>& damping_matrix)
	{
		/** \brief  Damping Effect Matrix
		*
		*/	

		// quadratic damping
	
		//damping_matrix = linear_damping + quadratic_damping * velocity) ; 
		// hydrodynamic damping simplified - vehicle perform non-coupled motion - only diagonal element is considered

		for (int i = 0; i < DOF; i++)
		{
			if (velocity(i) > -0.001)
				damping_matrix(i,i) =  param.linDampCoeff[i].positive + ( param.quadDampCoeff[i].positive * fabs(velocity(i)) );
			else
				damping_matrix(i,i) =  param.linDampCoeff[i].negative + ( param.quadDampCoeff[i].negative * fabs(velocity(i)) );
		}			
		
	}

	void DynamicModel::gravity_buoyancy(const Eigen::Vector3d &eulerang, Eigen::Matrix<double,6,1>& gravitybuoyancy)
	{
		float e1 = eulerang(0);
		float e2 = eulerang(1);
		float e3 = eulerang(2);
		float xg = param.distance_body2centerofgravity(0);
		float yg = param.distance_body2centerofgravity(1);
		float zg = param.distance_body2centerofgravity(2);
		float xb = param.distance_body2centerofbuoyancy(0);
		float yb = param.distance_body2centerofbuoyancy(1);
		float zb = param.distance_body2centerofbuoyancy(2);

		// currently its is assumed that the vehicle floats.i.e gravity = buoyancy 
		if (param.uwv_float == true)
			uwv_buoyancy = uwv_weight;

		gravitybuoyancy(0) =  (uwv_weight-uwv_buoyancy) * sin(e2);
		gravitybuoyancy(1) = -(uwv_weight-uwv_buoyancy) * (cos(e2)*sin(e1));
		gravitybuoyancy(2) = -(uwv_weight-uwv_buoyancy) * (cos(e2)*cos(e1));
		gravitybuoyancy(3) = -( ( (yg*uwv_weight)-(yb*uwv_buoyancy) ) * ( cos(e2)*cos(e1) ) ) + ( ( (zg*uwv_weight)-(zb*uwv_buoyancy) ) * (cos(e2)*sin(e1) ) );
		gravitybuoyancy(4) =  ( ( (zg*uwv_weight)-(zb*uwv_buoyancy) ) *   sin(e2) ) + ( ( (xg*uwv_weight)-(xb*uwv_buoyancy) ) *( cos(e2)*cos(e1) ) );
		gravitybuoyancy(5) =- ( ( (xg*uwv_weight)-(xb*uwv_buoyancy) ) * ( cos(e2)*sin(e1) ) ) - ( ( (yg*uwv_weight)-(yb*uwv_buoyancy) ) *sin(e2) );
	}

	void DynamicModel::gravity_buoyancy(const Eigen::Quaternion<double> q, Eigen::Matrix<double,6,1>& gravitybuoyancy)
	{
		// In Quaternion form
		float e1 = q.x();
		float e2 = q.y();
		float e3 = q.z();
		float e4 = q.w();
		float xg = param.distance_body2centerofgravity(0);
		float yg = param.distance_body2centerofgravity(1);
		float zg = param.distance_body2centerofgravity(2);
		float xb = param.distance_body2centerofbuoyancy(0);
		float yb = param.distance_body2centerofbuoyancy(1);
		float zb = param.distance_body2centerofbuoyancy(2);

		// currently its is assumed that the vehicle floats.i.e gravity = buoyancy 
		if (param.uwv_float == true)
			uwv_buoyancy = uwv_weight;

		gravitybuoyancy(0) = (2*((e4*e2)-(e1*e3)))*(uwv_weight-uwv_buoyancy);
		gravitybuoyancy(1) =-(2*((e4*e1)+(e2*e3)))*(uwv_weight-uwv_buoyancy);
		gravitybuoyancy(2) = (-sq(e4)+sq(e1)+sq(e2)-sq(e3))*(uwv_weight-uwv_buoyancy);
		gravitybuoyancy(3) = ((-sq(e4)+sq(e1)+sq(e2)-sq(e3))*((yg*uwv_weight)-(yb*uwv_buoyancy)))+(2*((e4*e1)+(e2*e3))*((zg*uwv_weight)-(zb*uwv_buoyancy)));
		gravitybuoyancy(4) =-((-sq(e4)+sq(e1)+sq(e2)-sq(e3))*((xg*uwv_weight)-(xb*uwv_buoyancy)))+(2*((e4*e2)-(e1*e3))*((zg*uwv_weight)-(zb*uwv_buoyancy)));
		gravitybuoyancy(5) =-(2*((e4*e1)+(e2*e3))*((xg*uwv_weight)-(xb*uwv_buoyancy)))-(2*((e4*e2)-(e1*e3))*((yg*uwv_weight)-(yb*uwv_buoyancy)));		
	}

	//void DynamicModel::thruster_ForceTorque(const Eigen::MatrixXd &thruster_control_matrix, const Eigen::MatrixXd &input_thrust, Eigen::MatrixXd &thrust)
	void DynamicModel::thruster_ForceTorque(Eigen::MatrixXd &thruster_control_matrix, const Eigen::MatrixXd &input_thrust, Eigen::MatrixXd &thrust)
	{
		thrust = thruster_control_matrix*input_thrust;
		
	}

	void DynamicModel::rot_BI_euler(const Eigen::Vector3d &eulerang, Eigen::Matrix3d& rot_BI)
	{

		rot_BI(0,0)  = ( cos(eulerang(2)) * cos(eulerang(1)) );
		rot_BI(0,1)  = ( sin(eulerang(2)) * cos(eulerang(1)) ); 
		rot_BI(0,2)  = ( -sin(eulerang(1)) );
		rot_BI(1,0)  = ( (-sin(eulerang(2)) * cos(eulerang(0)) ) + ( cos(eulerang(2)) * sin(eulerang(1)) * sin(eulerang(0)) ));
		rot_BI(1,1)  = ( ( cos(eulerang(2)) * cos(eulerang(0)) ) + ( sin(eulerang(2)) * sin(eulerang(1)) * sin(eulerang(0)) ));
		rot_BI(1,2)  = ( sin(eulerang(0)) * cos(eulerang(1)) );
		rot_BI(2,0)  = ( ( sin(eulerang(2)) * sin(eulerang(0)) ) + ( cos(eulerang(2)) * sin(eulerang(1)) * cos(eulerang(0)) ));
		rot_BI(2,1)  = ( (-cos(eulerang(2)) * cos(eulerang(0)) ) + ( sin(eulerang(2)) * sin(eulerang(1)) * cos(eulerang(0)) ));
		rot_BI(2,2)  = ( cos(eulerang(0)) * cos(eulerang(1)) );
	}

	void DynamicModel::rot_IB_euler(const Eigen::Vector3d &eulerang, Eigen::Matrix3d& rot_IB)
	{

		rot_IB(0,0)  = ( cos(eulerang(2)) * cos(eulerang(1)) );
		rot_IB(0,1)  = ( (-sin(eulerang(2)) * cos(eulerang(0)) ) + ( cos(eulerang(2)) * sin(eulerang(1)) * sin(eulerang(0)) ));
		rot_IB(0,2)  = ( ( sin(eulerang(2)) * sin(eulerang(0)) ) + ( cos(eulerang(2)) * sin(eulerang(1)) * cos(eulerang(0)) ));
		rot_IB(1,0)  = ( sin(eulerang(2)) * cos(eulerang(1)) ); 
		rot_IB(1,1)  = ( ( cos(eulerang(2)) * cos(eulerang(0)) ) + ( sin(eulerang(2)) * sin(eulerang(1)) * sin(eulerang(0)) ));
		rot_IB(1,2)  = ( -sin(eulerang(1)) );
		rot_IB(2,0)  = ( sin(eulerang(0)) * cos(eulerang(1)) );
		rot_IB(2,1)  = ( cos(eulerang(1)) * sin(eulerang(0)) );
		rot_IB(2,2)  = ( cos(eulerang(0)) * cos(eulerang(1)) );
	}

	void DynamicModel::rot_IB_euler(const Eigen::Matrix3d &rot_BI, Eigen::Matrix3d &rot_IB)
	{
		rot_IB = rot_BI.inverse();
	}

	void DynamicModel::jacobianMatrix_euler(const Eigen::Vector3d &eulerang, Eigen::Matrix3d& jacob_kin_e)
	{
		double c_pi = cos( eulerang(0) ); // Pi
		double s_pi = sin( eulerang(0) ); // Pi
		double c_th = cos( eulerang(1) ); // Theta
		double s_th = sin( eulerang(1) ); // Theta
		double c_ps = cos( eulerang(2) ); // Psi
	
		jacob_kin_e(0,0) = 1.0;		jacob_kin_e(0,1) = 0.0;		jacob_kin_e(0,2) = -s_th;
		jacob_kin_e(1,0) = 0.0;		jacob_kin_e(1,1) = c_ps;	jacob_kin_e(1,2) = c_th*s_pi;
		jacob_kin_e(2,0) = 0.0;		jacob_kin_e(2,1) = -s_pi;	jacob_kin_e(2,2) = c_th*c_pi;		

	}

	void DynamicModel::inverseJacobianMatrix_euler(const Eigen::Vector3d &eulerang, Eigen::Matrix3d& inv_jacob_kin_e)
	{
		double c_pi = cos( eulerang(0) ); 	// Pi
		double s_pi = sin( eulerang(0) ); 	// Pi
		double c_th = cos( eulerang(1) ); 	// Theta
		double s_th = sin( eulerang(1) ); 	// Theta

		assert(c_th!=0.0);		// Singularity
	
		inv_jacob_kin_e(0,0) = 1.0;	inv_jacob_kin_e(0,1) = s_pi*s_th;	inv_jacob_kin_e(0,2) = c_pi*s_th;
		inv_jacob_kin_e(1,0) = 0.0;	inv_jacob_kin_e(1,1) = c_pi*c_th;	inv_jacob_kin_e(1,2) = -c_th*s_pi;
		inv_jacob_kin_e(2,0) = 0.0;	inv_jacob_kin_e(2,1) = s_pi;		inv_jacob_kin_e(2,2) = c_pi;
	
		inv_jacob_kin_e = (1/c_th) * inv_jacob_kin_e;
	}

	void DynamicModel::compute_inv_jacob_e_RIB(const Eigen::Matrix3d & rot_IB, const Eigen::Matrix3d & inv_jacob_kin_e, Eigen::Matrix<double , 6, 6 > & inv_jacob_e_RIB)
	{
		inv_jacob_e_RIB << rot_IB, zero3x3, zero3x3, inv_jacob_kin_e;
	}



	void DynamicModel::compute_invTrans_jacob_e_RIB(const Eigen::Matrix3d & rot_IB, const Eigen::Matrix3d & inv_jacob_kin_e, Eigen::Matrix<double , 6, 6 > & invTrans_jacob_e_RIB)
	{
		
		//Transpose_3x3Matrix(inv_jacob_kin_e, TIJ_k_E);

		invTrans_jacob_e_RIB << rot_IB, zero3x3, zero3x3, inv_jacob_kin_e.transpose();

	}
	

	void DynamicModel::setPWMLevels(ThrusterMapping thrusters)
	{	
		for ( int j = 0; j < number_of_thrusters; j ++) 		// just to make sure the input set to zero before assigning the value
			ctrl_input[j] = 0.0;

		pwm_to_dc(thrusters, dc_volt);

		float factor = 0.01;
		
		for ( int i = 0; i < number_of_thrusters; i ++)
		{

			switch (thrusters.thruster_mapped_names.at(i))
			{
				case underwaterVehicle::SURGE:					
					if (( dc_volt.at(i) <= factor ) && (dc_volt.at(i) >=-factor))
						ctrl_input[i] = 0.0;
					else if ( dc_volt.at(i) < -factor )
						ctrl_input[i] = (param.thruster_coefficient_pwm.surge.negative.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.surge.negative.coefficient_b *dc_volt.at(i) ) ;						
					else
						ctrl_input[i] = (param.thruster_coefficient_pwm.surge.positive.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.surge.positive.coefficient_b * dc_volt.at(i) );
					break;				

				case underwaterVehicle::SWAY:
					if (( dc_volt.at(i) <= factor ) && (dc_volt.at(i) >=-factor))					
						ctrl_input[i] = 0.0;
					else if ( dc_volt.at(i) < -factor )
						ctrl_input[i] = (param.thruster_coefficient_pwm.sway.negative.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.sway.negative.coefficient_b * dc_volt.at(i) );
					else
						ctrl_input[i] = (param.thruster_coefficient_pwm.sway.positive.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.sway.positive.coefficient_b * dc_volt.at(i) );					
					break;

				case underwaterVehicle::HEAVE:
					if ( dc_volt.at(i) == 0.0 )
						ctrl_input[i] = 0.0;
					else if ( dc_volt.at(i) < -0.0 )
						ctrl_input[i] = (param.thruster_coefficient_pwm.heave.negative.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.heave.negative.coefficient_b * dc_volt.at(i) );
					else
						ctrl_input[i] = (param.thruster_coefficient_pwm.heave.positive.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.heave.positive.coefficient_b *  dc_volt.at(i) );					

					break;

				case underwaterVehicle::ROLL:
					if ( dc_volt.at(i) == 0.0 )
						ctrl_input[i] = 0.0;
					else if ( dc_volt.at(i) < -0.0 )
						ctrl_input[i] = (param.thruster_coefficient_pwm.roll.negative.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.roll.negative.coefficient_b * dc_volt.at(i) );
					else
						ctrl_input[i] = (param.thruster_coefficient_pwm.roll.positive.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.roll.positive.coefficient_b *  dc_volt.at(i) );					

					break;

				case underwaterVehicle::PITCH:
					if ( dc_volt.at(i) == 0.0 )
						ctrl_input[i] = 0.0;
					else if ( dc_volt.at(i) < -0.0 )
						ctrl_input[i] = (param.thruster_coefficient_pwm.pitch.negative.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.pitch.negative.coefficient_b * dc_volt.at(i) );
					else
						ctrl_input[i] = (param.thruster_coefficient_pwm.pitch.positive.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.pitch.positive.coefficient_b * dc_volt.at(i) );					
					break;

				case underwaterVehicle::YAW:
					if (( dc_volt.at(i) <= factor ) && (dc_volt.at(i) >=-factor))
						ctrl_input[i] = 0.0;
					else if ( dc_volt.at(i) < -factor )
						ctrl_input[i] = (param.thruster_coefficient_pwm.yaw.negative.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.yaw.negative.coefficient_b * dc_volt.at(i) );
					else
						ctrl_input[i] = (param.thruster_coefficient_pwm.yaw.positive.coefficient_a * (fabs(dc_volt.at(i)) * dc_volt.at(i))) + (param.thruster_coefficient_pwm.yaw.positive.coefficient_b * dc_volt.at(i) );					
					break;

			}
		}	
		
		/*std::cout<<"ctrl input = ";
		for(int i = 0; i< number_of_thrusters; i++)
			std::cout<<ctrl_input[i]<<" ";
		std::cout<<std::endl;*/
	

		// # of simulation steps to be performed by the RK4 algorith in one sampling interval
		//std::cout<<"param.sim_per_cycle  "<< param.sim_per_cycle<<std::endl;
		for (int ii=0; ii < param.sim_per_cycle ; ii++)		
		    solve();
	}

	// new method for testing
	/*void DynamicModel::setPWMLevels(ThrusterMapping thrusters)
	{	
		for ( int j = 0; j < number_of_thrusters; j ++) 		// just to make sure the input set to zero before assigning the value
			ctrl_input[j] = 0.0;

		pwm_to_dc(thrusters, dc_volt);

		for(int i=0;i<number_of_thrusters;i++)
			input_thrust(i,0) = dc_volt.at(i); 						// thrust as input 

		thruster_ForceTorque(thruster_control_matrix, input_thrust, temp_thrust);	// Calculating Thrust from the thruster value

		float factor = 0.01;

		if (( temp_thrust(0,0) <= factor ) && (temp_thrust(0,0) >=-factor))
			ctrl_input[0] = 0.0;
		else if ( temp_thrust(0,0) < -factor )
			ctrl_input[0] = (param.thruster_coefficient_pwm.surge.negative.coefficient_a * (fabs(temp_thrust(0,0)) * temp_thrust(0,0))) + (param.thruster_coefficient_pwm.surge.negative.coefficient_b *temp_thrust(0,0));
		else
			ctrl_input[0] = (param.thruster_coefficient_pwm.surge.positive.coefficient_a * (fabs(temp_thrust(0,0)) * temp_thrust(0,0))) + (param.thruster_coefficient_pwm.surge.positive.coefficient_b * temp_thrust(0,0));


		if (( temp_thrust(1,0) <= factor ) && (temp_thrust(1,0) >=-factor))
			ctrl_input[1] = 0.0;
		else if ( temp_thrust(1,0) < -factor )
			ctrl_input[1] = (param.thruster_coefficient_pwm.sway.negative.coefficient_a * (fabs(temp_thrust(1,0)) * temp_thrust(1,0))) + (param.thruster_coefficient_pwm.sway.negative.coefficient_b *temp_thrust(1,0));
		else
			ctrl_input[1] = (param.thruster_coefficient_pwm.sway.positive.coefficient_a * (fabs(temp_thrust(1,0)) * temp_thrust(1,0))) + (param.thruster_coefficient_pwm.sway.positive.coefficient_b * temp_thrust(1,0));

		if (( temp_thrust(2,0) <= factor ) && (temp_thrust(2,0) >=-factor))
			ctrl_input[2] = 0.0;
		else if ( temp_thrust(2,0) < -factor )
			ctrl_input[2] = (param.thruster_coefficient_pwm.heave.negative.coefficient_a * (fabs(temp_thrust(2,0)) * temp_thrust(2,0))) + (param.thruster_coefficient_pwm.heave.negative.coefficient_b *temp_thrust(2,0));
		else
			ctrl_input[2] = (param.thruster_coefficient_pwm.heave.positive.coefficient_a * (fabs(temp_thrust(2,0)) * temp_thrust(2,0))) + (param.thruster_coefficient_pwm.heave.positive.coefficient_b * temp_thrust(2,0));

		if (( temp_thrust(3,0) <= factor ) && (temp_thrust(3,0) >=-factor))
			ctrl_input[3] = 0.0;
		else if ( temp_thrust(3,0) < -factor )
			ctrl_input[3] = (param.thruster_coefficient_pwm.roll.negative.coefficient_a * (fabs(temp_thrust(3,0)) * temp_thrust(3,0))) + (param.thruster_coefficient_pwm.roll.negative.coefficient_b *temp_thrust(3,0));
		else
			ctrl_input[3] = (param.thruster_coefficient_pwm.roll.positive.coefficient_a * (fabs(temp_thrust(3,0)) * temp_thrust(3,0))) + (param.thruster_coefficient_pwm.roll.positive.coefficient_b * temp_thrust(3,0));

		if (( temp_thrust(4,0) <= factor ) && (temp_thrust(4,0) >=-factor))
			ctrl_input[4] = 0.0;
		else if ( temp_thrust(4,0) < -factor )
			ctrl_input[4] = (param.thruster_coefficient_pwm.pitch.negative.coefficient_a * (fabs(temp_thrust(4,0)) * temp_thrust(4,0))) + (param.thruster_coefficient_pwm.pitch.negative.coefficient_b *temp_thrust(4,0));
		else
			ctrl_input[4] = (param.thruster_coefficient_pwm.pitch.positive.coefficient_a * (fabs(temp_thrust(4,0)) * temp_thrust(4,0))) + (param.thruster_coefficient_pwm.pitch.positive.coefficient_b * temp_thrust(4,0));


		if (( temp_thrust(5,0) <= factor ) && (temp_thrust(5,0) >=-factor))
			ctrl_input[5] = 0.0;
		else if ( ctrl_input.at(5) < -factor )
			ctrl_input[5] = (param.thruster_coefficient_pwm.yaw.negative.coefficient_a * (fabs(temp_thrust(5,0)) * temp_thrust(5,0))) + (param.thruster_coefficient_pwm.yaw.negative.coefficient_b *temp_thrust(5,0));
		else
			ctrl_input[5] = (param.thruster_coefficient_pwm.yaw.positive.coefficient_a * (fabs(temp_thrust(5,0)) * temp_thrust(5,0))) + (param.thruster_coefficient_pwm.yaw.positive.coefficient_b * temp_thrust(5,0));

			
		
		
		
		//std::cout<<"ctrl input = ";
		//for(int i = 0; i< 6; i++)
		//	std::cout<<ctrl_input[i]<<" ";
		//std::cout<<std::endl;
	

		// # of simulation steps to be performed by the RK4 algorith in one sampling interval
		//std::cout<<"param.sim_per_cycle  "<< param.sim_per_cycle<<std::endl;
		for (int ii=0; ii < param.sim_per_cycle ; ii++)		
		    solve();
	}*/
	

	void DynamicModel::setRPMLevels(ThrusterMapping thrusters)
	{	
		for ( int j = 0; j < number_of_thrusters; j ++) 		// just to make sure the input set to zero before assigning the value
			ctrl_input[j] = 0.0;

		for ( int i = 0; i < number_of_thrusters; i ++)
		{
			switch (thrusters.thruster_mapped_names.at(i))
			{
				case underwaterVehicle::SURGE:
					if ( thrusters.thruster_value.at(i) < -0.0 )
						ctrl_input[i] = param.thruster_coefficient_rpm.surge.negative * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));
					else
						ctrl_input[i] = param.thruster_coefficient_rpm.surge.positive * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));	
					break;				

				case underwaterVehicle::SWAY:
					if ( thrusters.thruster_value.at(i) < -0.0 )
						ctrl_input[i] = param.thruster_coefficient_rpm.sway.negative * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));
					else
						ctrl_input[i] = param.thruster_coefficient_rpm.sway.positive * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));					
					break;

				case underwaterVehicle::HEAVE:
					if ( thrusters.thruster_value.at(i) < -0.0 )
						ctrl_input[i] = param.thruster_coefficient_rpm.heave.negative * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));
					else
						ctrl_input[i] = param.thruster_coefficient_rpm.heave.positive * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));					

					break;

				case underwaterVehicle::ROLL:
					if ( thrusters.thruster_value.at(i) < -0.0 )
						ctrl_input[i] = param.thruster_coefficient_rpm.roll.negative * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));
					else
						ctrl_input[i] = param.thruster_coefficient_rpm.roll.positive * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));					

					break;

				case underwaterVehicle::PITCH:
					if ( thrusters.thruster_value.at(i) < -0.0 )
						ctrl_input[i] = param.thruster_coefficient_rpm.pitch.negative * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));
					else
						ctrl_input[i] = param.thruster_coefficient_rpm.pitch.positive * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));					
					break;

				case underwaterVehicle::YAW:
					if ( thrusters.thruster_value.at(i) < -0.0 )
						ctrl_input[i] = param.thruster_coefficient_rpm.yaw.negative * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));
					else
						ctrl_input[i] = param.thruster_coefficient_rpm.yaw.positive * (fabs(thrusters.thruster_value.at(i)) * thrusters.thruster_value.at(i));					
					break;

			}
		}	
		
		// # of simulation steps to be performed by the RK4 algorith in one sampling interval
		
		for (int ii=0; ii < param.sim_per_cycle	; ii++)		
		    solve();		
	}

	
	void DynamicModel::pwm_to_dc(const ThrusterMapping thrusters, std::vector<double> & dc_volt)
	{	

		//std::cout<<"input = ";
		//for(int i = 0; i< thrusters.thruster_value.size(); i++)
		//	std::cout<<thrusters.thruster_value.at(i)<<" ";
		//std::cout<<std::endl;
	
		float factor = 0.01;	//for debugginh - need to be removed later


		// param.thrusterVolatage in volts - Thruster Maximum voltage
		for ( int i = 0; i < number_of_thrusters; i ++)
		{
			switch (thrusters.thruster_mapped_names.at(i))
			{
				case underwaterVehicle::SURGE:
					if ((thrusters.thruster_value.at(i) <= factor) && (thrusters.thruster_value.at(i) >=-factor)) 
						dc_volt.at(i) = 0.0;
					else if ( thrusters.thruster_value.at(i) < -factor)
						dc_volt.at(i) = ((((param.maxSurgePWM - param.minSurgePWM) * thrusters.thruster_value.at(i)) - param.minSurgePWM) / 255.0) * param.thrusterVoltage;						
					else if( thrusters.thruster_value.at(i) > factor) 
						dc_volt.at(i) = ((((param.maxSurgePWM - param.minSurgePWM) * thrusters.thruster_value.at(i)) + param.minSurgePWM) / 255.0) * param.thrusterVoltage;
				
					break;				

				case underwaterVehicle::SWAY:
					if ((thrusters.thruster_value.at(i) <= factor) && (thrusters.thruster_value.at(i) >=-factor)) 
						dc_volt.at(i) = 0.0;
            				else if ( thrusters.thruster_value.at(i) < -factor)		
						dc_volt.at(i) = ((((param.maxSwayPWM - param.minSwayPWM) * thrusters.thruster_value.at(i)) - param.minSwayPWM) / 255.0) * param.thrusterVoltage;
					else if ( thrusters.thruster_value.at(i) > factor)  
						dc_volt.at(i) = ((((param.maxSwayPWM - param.minSwayPWM) * thrusters.thruster_value.at(i)) + param.minSwayPWM) / 255.0) * param.thrusterVoltage;
					break;				

				case underwaterVehicle::HEAVE:
					if ((thrusters.thruster_value.at(i) <= 0.0001) && (thrusters.thruster_value.at(i) >=-0.0001))
							dc_volt.at(i) = 0.0;		
					else if ( thrusters.thruster_value.at(i) < -0.001)		
						dc_volt.at(i) = ((((param.maxHeavePWM - param.minHeavePWM) * thrusters.thruster_value.at(i)) - param.minHeavePWM) / 255.0) * param.thrusterVoltage;
					else if ( thrusters.thruster_value.at(i) > 0.001)
						dc_volt.at(i) = ((((param.maxHeavePWM - param.minHeavePWM) * thrusters.thruster_value.at(i)) + param.minHeavePWM) / 255.0) * param.thrusterVoltage;
					
					break;	

				case underwaterVehicle::ROLL:
					if (thrusters.thruster_value.at(i) == 0)
						dc_volt.at(i) = 0.0;
					else if ( thrusters.thruster_value.at(i) < -0.001)		
						dc_volt.at(i) = ((((param.maxRollPWM - param.minRollPWM) * thrusters.thruster_value.at(i)) - param.minRollPWM) / 255.0) * param.thrusterVoltage;
					else if ( thrusters.thruster_value.at(i) > 0.001)
						dc_volt.at(i) = ((((param.maxRollPWM - param.minRollPWM) * thrusters.thruster_value.at(i)) + param.minRollPWM) / 255.0) * param.thrusterVoltage;
					
					break;	

				case underwaterVehicle::PITCH:
					if (thrusters.thruster_value.at(i) == 0)
						dc_volt.at(i) = 0.0;
					else if ( thrusters.thruster_value.at(i) < negative_constant)		
						dc_volt.at(i) = ((((param.maxPitchPWM - param.minPitchPWM) * thrusters.thruster_value.at(i)) - param.minPitchPWM) / 255.0) * param.thrusterVoltage;
					else if ( thrusters.thruster_value.at(i) > negative_constant)
						dc_volt.at(i) = ((((param.maxPitchPWM - param.minPitchPWM) * thrusters.thruster_value.at(i)) + param.minPitchPWM) / 255.0) * param.thrusterVoltage;
					break;	

				case underwaterVehicle::YAW:
					if ((thrusters.thruster_value.at(i) <= factor) && (thrusters.thruster_value.at(i) >=-factor))
						dc_volt.at(i) = 0.0;
					else if ( thrusters.thruster_value.at(i) < -factor)		
						dc_volt.at(i) = ((((param.maxYawPWM - param.minYawPWM) * thrusters.thruster_value.at(i)) - param.minYawPWM) / 255.0) * param.thrusterVoltage;
					else if ( thrusters.thruster_value.at(i) > factor)
						dc_volt.at(i) = ((((param.maxYawPWM - param.minYawPWM) * thrusters.thruster_value.at(i)) + param.minYawPWM) / 255.0) * param.thrusterVoltage;

					break;	

			}
		}
							
	}			
	
	//DERIV (current_time, plant_state, ctrl_input, f1);
	//void DynamicModel :: DERIV(const double t, const double *x, const double *u, double *xdot)
	void DynamicModel :: DERIV(const double t, double *x, const double *u, double *xdot)
	{
		// underwater vehicle  - Dynamics
		/*  V(0) = u    V_dot(0) = u_dot        X(0) = u    X_dot(0) = u_dot    X(6) = x       X_dot(6) = u
		    V(1) = v    V_dot(1) = v_dot        X(1) = v    X_dot(1) = v_dot    X(7) = y       X_dot(7) = v
		    V(2) = w    V_dot(2) = w_dot        X(2) = w    X_dot(2) = w_dot    X(8) = z       X_dot(8) = w
		    V(3) = p    V_dot(3) = p_dot        X(3) = p    X_dot(3) = p_dot    X(9) = rot x   X_dot(9) = p
		    V(4) = q    V_dot(4) = q_dot        X(4) = q    X_dot(4) = q_dot    X(10)= rot y   X_dot(10)= q
		    V(5) = r    V_dot(5) = r_dot        X(5) = r    X_dot(5) = r_dot    x(11)= rot z   X_dot(11)= r*/

		for(int i=0;i<6;i++)
			velocity(i) = x[i]; 							// velocity


			
		for(int i=0;i<3;i++)
		{
			position(i)	= x[i+6];						// position			
			
			if (x[i+9] > PI)
			{
				orientation_euler(i) 	= x[i+9] - 2*PI;			// orientation	
				x[i+9]			= x[i+9] - 2*PI;
			}			 
			else if (x[i+9] < -PI)
			{
				orientation_euler(i) 	= x[i+9] + 2*PI;
				x[i+9]		 	= x[i+9] + 2*PI;

			}
			else
				orientation_euler(i) = x[i+9];  
		}
 

		for(int i=0;i<number_of_thrusters;i++)
			input_thrust(i,0) = u[i]; 						// thrust as input 

		// The below vehicle dynamics is w.r.t to Body-fixed frame		
		gravity_buoyancy(orientation_euler, gravitybuoyancy_bff);			
		hydrodynamic_damping(velocity, damping_matrix_bff);		
		thruster_ForceTorque(thruster_control_matrix, input_thrust, thrust_bff);	// Calculating Thrust from the thruster value

		//for testing
		/*
		for(int i=0;i<DOF;i++)
			thrust_bff(i,0) = u[i]; 
		*/
		inertia_matrix(velocity, mass_matrix_bff);


		// simplified equation of motion for an underwater vehicle in BFF
		Inv_massMatrix = mass_matrix_bff.inverse();
		acceleration  = Inv_massMatrix * ( thrust_bff - (damping_matrix_bff * velocity) - gravitybuoyancy_bff );


		// Now the vehicle dynamics is represented w.r.t Earth-fixed frame
		//rot_BI_euler (orientation_euler, rot_BI);					// Computing rotation matrix for body fixed frame w.r.t earth fixed frame
		//rot_IB_euler (rot_BI, rot_IB);							// Computing rotation matrix for earth fixed frame w.r.t body fixed frame
/*		rot_IB_euler (orientation_euler, rot_IB);					// Computing rotation matrix for body fixed frame w.r.t earth fixed frame

		jacobianMatrix_euler(orientation_euler, jacob_kin_e);				// Computing Jacobian matrix
		inverseJacobianMatrix_euler(orientation_euler, inv_jacob_kin_e);		// Computing Inverse of Jacobian matrix
		compute_inv_jacob_e_RIB (rot_IB, inv_jacob_kin_e, inv_jacob_e_RIB);		// Computing Inverse of Jacobian matrix w.r.t. rot _IB
		compute_invTrans_jacob_e_RIB(rot_IB, inv_jacob_kin_e, invTrans_jacob_e_RIB);	// Computing Transpose of Inverse Jacobian matrix w.r.t. rot_IB
		
		mass_matrix_eff		= invTrans_jacob_e_RIB * mass_matrix_bff * inv_jacob_e_RIB;
		damping_matrix_eff	= invTrans_jacob_e_RIB * damping_matrix_bff * inv_jacob_e_RIB;
		gravitybuoyancy_eff 	= invTrans_jacob_e_RIB * gravitybuoyancy_bff;
		thrust_eff 		= invTrans_jacob_e_RIB * thrust_bff;
	
		Inv_massMatrix = mass_matrix_eff.inverse();
		
		// simplified equation of motion for an underwater vehicle
		acceleration  = Inv_massMatrix * ( thrust_eff - (damping_matrix_eff * velocity) - gravitybuoyancy_eff );

*/		

		for(int i=0;i<6;i++)
		{
		    xdot[i]  = acceleration(i);
		    xdot[i+6]= x[i];	    
		}

		for(int i=0;i<3;i++)
		{
			// velocity.head(3) can also be used, but no idea which is efficient
			linear_velocity(i) 	= velocity(i);	
			angular_velocity(i) 	= velocity(i+3);
			linear_acceleration(i)  = acceleration(i);
			angular_acceleration(i) = acceleration(i+3);
		}

		

		/*	
		std::cout<<u[0]<<" "<<u[1]<<" "<<u[2]<<" "<<u[3]<<" "<<u[4]<<" "<<u[5]<<std::endl;
		std::cout<<"-----------"<<std::endl;
		std::cout<<"thrust_bff";
		std::cout<<thrust_bff(0)<<" "<<thrust_bff(1)<<" "<<thrust_bff(2)<<" "<<thrust_bff(3)<<" "<<thrust_bff(4)<<" "<<thrust_bff(5)<<std::endl;


		//std::cout<<"tb= "<<thrust<<std::endl;		
		//std::cout<<"mass= "<< mass_matrix<<std::endl;
		std::cout << "mass_matrix_bff= " << mass_matrix_bff<<std::endl;
		std::cout<<"inv mass= "<<Inv_massMatrix<<std::endl;		
		std::cout<<"Dp= "<<(damping_matrix_bff)<<std::endl;	
		std::cout<<"V "<<velocity<<std::endl;
		std::cout<<"V_dot "<<acceleration<<std::endl;
		//std::cout<<"tb= "<<thrust(0)<<" "<<thrust(1)<<" "<<thrust(2)<<" "<<thrust(3)<<" "<<thrust(4)<<" "<<thrust(5)<<std::endl;
		//std::cout<<"Dp= "<<(damping_matrix)<<std::endl;		
		std::cout<<"V "<<velocity(0)<<" "<<velocity(1)<<" "<<velocity(2)<<" "<<velocity(3)<<" "<<velocity(4)<<" "<<velocity(5)<<std::endl;
		std::cout<<"V_dot "<<acceleration(0)<<" "<<acceleration(1)<<" "<<acceleration(2)<<" "<<acceleration(3)<<" "<<acceleration(4)<<" "<<acceleration(5)<<std::endl;
		*/
	}
	
	
	void DynamicModel::setPosition(base::Vector3d v){
	  position = v;
	}
	
	void DynamicModel::setLinearVelocity(base::Vector3d v){
	  linear_velocity = v;
	  
	}
	
	void DynamicModel::setSamplingtime(double dt){
	  integration_step = dt/param.sim_per_cycle;
	}  
	  
};

	

	
